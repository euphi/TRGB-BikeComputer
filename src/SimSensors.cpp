/*
 * SimSensors.cpp
 *
 *  Created on: 29.09.2026
 *
 * See SimSensors.h.
 */

#ifdef BC_SIM

#include "SimSensors.h"
#include "Singletons.h"
#include "Stats/Distance.h"
#include "WebPage.h"
#include <ArduinoJson.h>
#include <math.h>

// Advances a revolution position by rate * dt. When a new whole revolution is completed, the
// event time moves to the moment it was -- what a real CSC sensor reports along with the
// counter -- so the firmware's revolutions/time division gives back exactly the set speed.
static void advance(double& pos, double& eventMs, double revsPerS, float dtS, uint32_t now) {
	if (revsPerS <= 0) return;
	const double before = floor(pos);
	pos += revsPerS * dtS;
	const double whole = floor(pos);
	if (whole > before) eventMs = now - (pos - whole) / revsPerS * 1000.0;
}

// CSC event time: 1/1024 s, wrapping at 16 bit. 0 means "no timestamp" to Distance::updateRevs().
static uint16_t ticks1024(double ms) {
	uint16_t t = static_cast<uint16_t>(static_cast<uint64_t>(ms * 1.024) & 0xFFFF);
	return t ? t : 1;
}

void SimSensors::setup() {
	Command simCmd = console.addCmd("sim", +[](cmd* c) {
		Command command(c);
		sim.handleCommand(command.getArgument(0).getValue(), command.getArgument(1).getValue(), command.getArgument(2).getValue());
	}, +[](uint8_t pos, SerialConsole::Matches& m) {
		if (pos == 1) {m.add("status"); m.add("stop");}
	});
	simCmd.addPositionalArgument("a", "status");
	simCmd.addPositionalArgument("b", "");
	simCmd.addPositionalArgument("c", "");
	simCmd.setDescription("Sensor simulator: sim <km/h> [<rpm>|-] [<bpm>|-] | sim stop | sim status");

	// Action routes before "/debug/sim": a plain route also matches "<uri>/..." (see /debug/imu)
	AsyncWebServer& server = webserver.getServer();
	server.on("/debug/sim/set", HTTP_GET, [this](AsyncWebServerRequest *request) {
		auto value = [request](const char* name, float dflt) -> float {
			if (!request->hasParam(name)) return dflt;
			String v = request->getParam(name)->value();
			v.trim();
			return (v.isEmpty() || v == "-") ? dflt : v.toFloat();
		};
		const float speed = value("speed", NAN);
		if (isnan(speed) || speed < 0 || speed > 120) {
			request->send(400, "text/plain", "speed missing or out of range (0..120 km/h)");
			return;
		}
		set(speed, static_cast<int16_t>(value("cad", -1)), static_cast<int16_t>(value("hr", -1)));
		request->send(200, "text/plain", "OK");
	});
	server.on("/debug/sim/stop", HTTP_GET, [this](AsyncWebServerRequest *request) {
		stop();
		request->send(200, "text/plain", "Stopped");
	});
	server.on("/debug/sim.json", HTTP_GET, [this](AsyncWebServerRequest *request) {
		String json;
		getJson(json);
		request->send(200, "application/json", json);
	});
	server.on("/debug/sim", HTTP_GET, [this](AsyncWebServerRequest *request) {
		String html;
		getPage(html);
		request->send(200, "text/html", html);
	});

	// A real sensor notifies about once a second. Runs in the esp_timer task like
	// Statistics::cycle(), so everything that touches stats happens in tick(), not in the
	// CLI or web task that called set()/stop().
	ticker.attach_ms(1000, +[](SimSensors* s) {s->tick();}, this);
	bclog.log(BCLogger::Log_Warn, BCLogger::TAG_OP, "SIMULATOR BUILD: speed/cadence/HR can be faked (serial \"sim\", web /debug/sim)");
}

void SimSensors::set(float _speedKmh, int16_t _cadenceRpm, int16_t _hr) {
	portENTER_CRITICAL(&mux);
	speedKmh = constrain(_speedKmh, 0.0f, 120.0f);
	cadenceRpm = _cadenceRpm < 0 ? -1 : min<int16_t>(_cadenceRpm, 250);
	hr = _hr < 0 ? -1 : min<int16_t>(_hr, 250);
	active = true;
	updates++;
	portEXIT_CRITICAL(&mux);
	fedByTrailBridge = false;	// a manual setting takes over; feedFromTrailBridge() marks its own after this
	simHeightM = NAN;
	simPowerW = -1;
}

void SimSensors::stop() {
	active = false;
}

void SimSensors::feedFromTrailBridge(bool simulated, float _speedKmh, int16_t _cadenceRpm, int16_t _hr, float heightM, int32_t powerW) {
	if (!simulated) {
		endTrailBridgeFeed();
		return;
	}
	if (!fedByTrailBridge) bclog.log(BCLogger::Log_Info, BCLogger::TAG_OP, "Simulator fed by TrailBridge test ride");
	set(_speedKmh, _cadenceRpm, _hr);
	simHeightM = heightM;
	simPowerW = powerW;
	fedByTrailBridge = true;
}

void SimSensors::endTrailBridgeFeed() {
	if (!fedByTrailBridge) return;
	fedByTrailBridge = false;
	simHeightM = NAN;
	simPowerW = -1;
	stop();
	bclog.log(BCLogger::Log_Info, BCLogger::TAG_OP, "TrailBridge test ride ended");
}

void SimSensors::tick() {
	portENTER_CRITICAL(&mux);
	const bool act = active;
	const float v = speedKmh;
	const int16_t cad = cadenceRpm, bpm = hr;
	portEXIT_CRITICAL(&mux);
	const uint32_t now = millis();

	if (!act) {
		if (running) {
			// Like switching the sensors off -- what BLEDevices::updateDisconnectedDev() does
			running = false;
			stats.setConnected(false);
			if (cadenceSent) stats.addCadence(-1, 0);
			if (hrSent) stats.addHR(-1);
			cadenceSent = hrSent = false;
			bclog.log(BCLogger::Log_Info, BCLogger::TAG_OP, "Simulator stopped - sensors disconnected");
		}
		return;
	}
	if (!running) {
		running = true;
		lastTickMs = now;
		if (wheelEventMs == 0) wheelEventMs = crankEventMs = now;
		bclog.log(BCLogger::Log_Info, BCLogger::TAG_OP, "Simulator started");
	}
	const float dt = (now - lastTickMs) / 1000.0f;
	lastTickMs = now;

	const float wheelC = stats.getDistHandler().revsToDistance(1);
	if (wheelC > 0) advance(wheelPos, wheelEventMs, v / 3.6 / wheelC, dt, now);
	const uint32_t revs = static_cast<uint32_t>(wheelPos);
	uint16_t t = ticks1024(wheelEventMs);
	uint8_t speedPkt[7] = {0x01, (uint8_t)revs, (uint8_t)(revs >> 8), (uint8_t)(revs >> 16), (uint8_t)(revs >> 24), (uint8_t)t, (uint8_t)(t >> 8)};
	bleDevs.simNotifyCSC(true, speedPkt, sizeof(speedPkt));

	if (cad >= 0) {
		advance(crankPos, crankEventMs, cad / 60.0, dt, now);
		const uint16_t crank = static_cast<uint16_t>(static_cast<uint32_t>(crankPos));
		t = ticks1024(crankEventMs);
		uint8_t cadPkt[5] = {0x02, (uint8_t)crank, (uint8_t)(crank >> 8), (uint8_t)t, (uint8_t)(t >> 8)};
		bleDevs.simNotifyCSC(false, cadPkt, sizeof(cadPkt));
		cadenceSent = true;
	} else if (cadenceSent) {
		stats.addCadence(-1, 0);
		cadenceSent = false;
	}

	if (bpm >= 0) {
		uint8_t hrPkt[2] = {0x00, (uint8_t)bpm};
		bleDevs.simNotifyHR(hrPkt, sizeof(hrPkt));
		hrSent = true;
	} else if (hrSent) {
		stats.addHR(-1);
		hrSent = false;
	}
}

void SimSensors::handleCommand(const String& a, const String& b, const String& c) {
	if (a.equalsIgnoreCase("status")) {
		printStatus();
		return;
	}
	if (a.equalsIgnoreCase("stop")) {
		stop();
		Serial.println("Simulator: sensors off");
		return;
	}
	auto opt = [](const String& s) -> int16_t {return (s.isEmpty() || s == "-") ? -1 : static_cast<int16_t>(s.toInt());};
	const float speed = a.toFloat();
	if ((speed == 0 && a[0] != '0') || speed < 0 || speed > 120) {
		Serial.println("Usage: sim <km/h 0..120> [<rpm>|-] [<bpm>|-] | sim stop | sim status");
		return;
	}
	set(speed, opt(b), opt(c));
}

void SimSensors::printStatus() {
	portENTER_CRITICAL(&mux);
	const bool act = active;
	const float v = speedKmh;
	const int16_t cad = cadenceRpm, bpm = hr;
	portEXIT_CRITICAL(&mux);
	Serial.printf("Simulator %s: %.1f km/h, cadence %d, HR %d (-1 = off) | wheel %lu revs, crank %u revs, %lu updates\r\n",
			act ? "ACTIVE" : "off", v, cad, bpm, static_cast<unsigned long>(wheelPos), static_cast<unsigned>(crankPos),
			static_cast<unsigned long>(updates));
	if (fedByTrailBridge) {
		Serial.printf("From TrailBridge: height %.1f m (NaN = none), power %ld W (-1 = none)\r\n",
				static_cast<float>(simHeightM), static_cast<long>(simPowerW));
	}
	Serial.printf("Statistics: state %s, distance since power-on %lu m\r\n",
			Statistics::PREF_TIME_STRING[stats.getCurDriveState()] + 8, static_cast<unsigned long>(stats.getDistance(Statistics::SUM_ESP_START)));
}

void SimSensors::getJson(String& out) {
	JsonDocument doc;
	portENTER_CRITICAL(&mux);
	doc["active"] = active;
	doc["speed"] = speedKmh;
	doc["cad"] = cadenceRpm;
	doc["hr"] = hr;
	portEXIT_CRITICAL(&mux);
	doc["updates"] = updates;
	doc["source"] = fedByTrailBridge ? "trailbridge" : "manual";
	doc["height"] = simHeightM;		// NAN: serialised as null
	doc["power"] = simPowerW;
	doc["wheelRevs"] = static_cast<uint32_t>(wheelPos);
	doc["crankRevs"] = static_cast<uint32_t>(crankPos);
	doc["wheelC"] = stats.getDistHandler().revsToDistance(1);
	doc["measured"] = stats.getSpeed();
	serializeJson(doc, out);
}

void SimSensors::getPage(String& out) {
	WebPage::begin(out, "Sensor Simulator",
		"input[type=number]{width:6em}input{margin-right:8px}td.n{text-align:right}"
		"#gpxInfo,#playInfo{margin:6px 0}");
	out += F(
		"<p class=\"eyebrow\">Simulator build: fakes a CSC speed sensor, a cadence sensor and a heart-rate strap. "
		"Their notifications go through the normal BLE parser, so distance and statistics run the real code. "
		"Real sensors are ignored while the simulator is active. Statistics are kept apart from the normal firmware's "
		"(own NVS namespace).</p>\n"
		"<h3>Manual</h3>\n"
		"<div class=\"row\">"
		"<label>km/h <input type=\"number\" id=\"sp\" min=\"0\" max=\"120\" step=\"0.5\" value=\"20\"></label>"
		"<label>rpm <input type=\"number\" id=\"cd\" min=\"0\" max=\"250\" placeholder=\"off\" value=\"80\"></label>"
		"<label>bpm <input type=\"number\" id=\"hr\" min=\"0\" max=\"250\" placeholder=\"off\"></label>"
		"</div>\n"
		"<div class=\"row\" style=\"margin-top:8px;\">"
		"<a class=\"btn\" href=\"#\" onclick=\"setManual();return false;\">Set</a>"
		"<a class=\"btn btn-ghost\" href=\"#\" onclick=\"setV(0);return false;\">Speed 0</a>"
		"<a class=\"btn btn-ghost\" href=\"#\" onclick=\"stopAll();return false;\">Sensors off</a>"
		"</div>\n"
		"<h3>GPX playback</h3>\n"
		"<p class=\"eyebrow\">Speed from the track geometry (smoothed over 4 s), heart rate and cadence from the "
		"TrackPointExtension if present. Without cadence in the file: the value below while moving, 0 below 3 km/h and "
		"downhill steeper than 4 % (coasting). Real time only -- keep this tab in the foreground, background tabs are "
		"throttled. Same rules as <code>python3 -m bikelog sim</code>.</p>\n"
		"<div class=\"row\"><input type=\"file\" id=\"gpx\" accept=\".gpx\" onchange=\"loadGpx(this.files[0])\"></div>\n"
		"<div class=\"row\" style=\"margin-top:8px;\">"
		"<label>Cadence if none <input type=\"number\" id=\"defCad\" value=\"80\"></label>"
		"<label>Max. gap s <input type=\"number\" id=\"maxGap\" value=\"0\" title=\"0 = keep pauses as recorded\"></label>"
		"<label>Start at min <input type=\"number\" id=\"startMin\" value=\"0\"></label>"
		"</div>\n"
		"<div id=\"gpxInfo\" class=\"eyebrow\">No file loaded.</div>\n"
		"<div class=\"row\">"
		"<a class=\"btn\" href=\"#\" onclick=\"play();return false;\">Play</a>"
		"<a class=\"btn btn-ghost\" href=\"#\" onclick=\"stopPlay(true);return false;\">Stop</a>"
		"</div>\n"
		"<div id=\"playInfo\" class=\"eyebrow\"></div>\n"
		"<h3>Status</h3>\n<table><tbody>\n"
		"<tr><td>Simulator</td><td id=\"simSt\"></td></tr>\n"
		"<tr><td>Speed measured (km/h)</td><td class=\"n\" id=\"meas\"></td></tr>\n"
		"<tr><td>Ride state</td><td id=\"drv\"></td></tr>\n"
		"</tbody></table>\n"
		"<table><thead><tr><th></th><th>Start</th><th>Trip</th><th>Tour</th></tr></thead><tbody id=\"sum\"></tbody></table>\n"
		"<div class=\"row\" style=\"margin-top:8px;\"><a class=\"btn btn-ghost\" href=\"/stat/statistics.html\">Statistics page</a></div>\n");
	WebPage::end(out,
		"const $=i=>document.getElementById(i);\n"
		"function q(v){return (v===''||v==null)?'-':v;}\n"
		"function send(v,c,h){return fetch('/debug/sim/set?speed='+v.toFixed(2)+'&cad='+q(c)+'&hr='+q(h));}\n"
		"function setManual(){req('/debug/sim/set?speed='+$('sp').value+'&cad='+q($('cd').value)+'&hr='+q($('hr').value),'Set');}\n"
		"function setV(v){$('sp').value=v;setManual();}\n"
		"function stopAll(){stopPlay(false);req('/debug/sim/stop','Sensors off');}\n"
		// --- GPX: same rules as Tools/bikelog/sim.py
		"let trk=null,timer=null,t0=0,sent=0,errs=0;\n"
		"function hav(a,b,c,d){const r=Math.PI/180,x=Math.sin((c-a)*r/2),y=Math.sin((d-b)*r/2);"
		"return 2*6371000*Math.asin(Math.sqrt(x*x+Math.cos(a*r)*Math.cos(c*r)*y*y));}\n"
		"function loadGpx(f){if(!f)return;f.text().then(txt=>{\n"
		" const doc=new DOMParser().parseFromString(txt,'application/xml');\n"
		" const g=(p,n)=>{const e=p.getElementsByTagNameNS('*',n)[0];return e&&e.textContent.trim()!==''?+e.textContent:null;};\n"
		" let pts=[],prev=null,d=0,hasCad=false,hasHr=false;\n"
		" for(const p of doc.getElementsByTagNameNS('*','trkpt')){\n"
		"  const te=p.getElementsByTagNameNS('*','time')[0];if(!te)continue;\n"
		"  const t=Date.parse(te.textContent)/1000;if(prev&&t<=prev.t)continue;\n"
		"  const lat=+p.getAttribute('lat'),lon=+p.getAttribute('lon');\n"
		"  if(prev)d+=hav(prev.lat,prev.lon,lat,lon);\n"
		"  const o={t,lat,lon,d,ele:g(p,'ele'),hr:g(p,'hr'),cad:g(p,'cad')};\n"
		"  hasCad=hasCad||o.cad!=null;hasHr=hasHr||o.hr!=null;pts.push(o);prev=o;}\n"
		" if(pts.length<2){toast('No timed track points in the file',true);return;}\n"
		" trk={raw:pts,hasCad,hasHr};prepare();\n"
		"});}\n"
		// playback time line: pauses longer than maxGap shortened to maxGap
		"function prepare(){const gap=+$('maxGap').value||0;let s=0;const p=trk.raw;p[0].s=0;\n"
		" for(let i=1;i<p.length;i++){let dt=p[i].t-p[i-1].t;if(gap>0&&dt>gap)dt=gap;s+=dt;p[i].s=s;}\n"
		" trk.T=s;const km=(p[p.length-1].d/1000).toFixed(2),mn=Math.round(s/60);\n"
		" $('gpxInfo').textContent=p.length+' points, '+km+' km, '+mn+' min playback'+(trk.hasCad?', cadence':'')+(trk.hasHr?', heart rate':'');}\n"
		"function idx(s){const p=trk.raw;let lo=0,hi=p.length-1;while(hi-lo>1){const m=(lo+hi)>>1;if(p[m].s<=s)lo=m;else hi=m;}return lo;}\n"
		"function at(s,k){const p=trk.raw;s=Math.max(0,Math.min(trk.T,s));const i=idx(s),a=p[i],b=p[Math.min(i+1,p.length-1)];\n"
		" if(a[k]==null||b[k]==null||b.s==a.s)return a[k];return a[k]+(b[k]-a[k])*(s-a.s)/(b.s-a.s);}\n"
		"function sample(s){const a=Math.max(0,s-2),b=Math.min(trk.T,s+2);\n"
		" const v=b>a?(at(b,'d')-at(a,'d'))/(b-a)*3.6:0;const p=trk.raw[idx(s)];\n"
		" let cad=null;if(trk.hasCad)cad=p.cad;else{const c=Math.max(0,s-5),e=Math.min(trk.T,s+5),dd=at(e,'d')-at(c,'d');\n"
		"  const e1=at(c,'ele'),e2=at(e,'ele'),gr=(dd>20&&e1!=null&&e2!=null)?(e2-e1)/dd*100:0;\n"
		"  cad=(v<3||gr<-4)?0:(+$('defCad').value||0);}\n"
		" return {v,cad:cad==null?null:Math.round(cad),hr:trk.hasHr&&p.hr!=null?Math.round(p.hr):null,d:at(s,'d')};}\n"
		"function play(){if(!trk){toast('Load a GPX file first',true);return;}stopPlay(false);prepare();\n"
		" t0=Date.now()-(+$('startMin').value||0)*60000;sent=0;errs=0;tick();timer=setInterval(tick,1000);}\n"
		"function tick(){const s=(Date.now()-t0)/1000;if(s>trk.T){stopPlay(true);toast('GPX playback finished');return;}\n"
		" const x=sample(s);send(x.v,x.cad,x.hr).then(r=>{if(!r.ok)errs++;}).catch(()=>errs++);sent++;\n"
		" $('playInfo').textContent=Math.floor(s/60)+':'+String(Math.floor(s%60)).padStart(2,'0')+' / '+Math.round(trk.T/60)+' min, '"
		"+(x.d/1000).toFixed(2)+' km, '+x.v.toFixed(1)+' km/h, cad '+q(x.cad)+', hr '+q(x.hr)+(errs?', '+errs+' errors':'');}\n"
		"function stopPlay(off){if(timer){clearInterval(timer);timer=null;if(off)fetch('/debug/sim/stop');}}\n"
		// --- status
		"const rows=[['Distance (m)',o=>o.dist.net],['Lost (m)',o=>o.dist.lost],['Moving (s)',o=>o.time.moving],"
		"['Ride (s)',o=>o.time.ride],['Cruise (s)',o=>o.time.cruise],['Coast (s)',o=>o.time.coast],['Stops (s)',o=>o.time.stops],"
		"['Breaks (s)',o=>o.time.breaks],['Total (s)',o=>o.time.total],['No conn. (s)',o=>o.time.noconn],"
		"['Avg all',o=>o.avg.ALL],['Avg moving',o=>o.avg.DRIVE],['Avg w/o breaks',o=>o.avg.NOBREAK],['Avg w/o cruise',o=>o.avg.NOCRUISE],"
		"['Max km/h',o=>o.maxSpeed],['Avg cadence',o=>o.cadence]];\n"
		"function f(v){return v==null?'-':(typeof v==='number'&&!Number.isInteger(v)?v.toFixed(2):v);}\n"
		"async function poll(){try{const s=await (await fetch('/debug/sim.json')).json();\n"
		" $('simSt').textContent=s.active?('active: '+s.speed.toFixed(1)+' km/h, cad '+(s.cad<0?'off':s.cad)+', hr '+(s.hr<0?'off':s.hr)"
		"+' ('+s.wheelRevs+' wheel revs, '+s.wheelC.toFixed(3)+' m)'):'off';\n"
		" $('meas').textContent=f(s.measured);\n"
		" const m=await (await fetch('/stat/summary')).json();\n"
		" $('drv').textContent=m.state.drive+(m.state.rideMode?' (Ride mode)':' (FreeRide)')+(m.state.session?', session open':'');\n"
		" $('sum').innerHTML=rows.map(r=>'<tr><td>'+r[0]+'</td>'+['START','TRIP','TOUR'].map(k=>'<td class=\"n\">'+f(r[1](m[k]))+'</td>').join('')+'</tr>').join('');\n"
		"}catch(e){}}\n"
		"poll();setInterval(poll,2000);\n");
}

#endif	// BC_SIM

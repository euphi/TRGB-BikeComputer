/*
 * WifiRoutes.cpp
 *
 * The /wifi page and its JSON interface (WifiWebserver::setupWifiRoutes()):
 *
 *   GET  /wifi            the page: saved networks (reorder = change priority), scan, add, hotspot
 *   GET  /wifi/state      {phase, ssid, ip, rssi, scanning, networks[], ap{ssid}, max}
 *   GET  /wifi/scan       {scanning, version, results[{ssid, rssi, open, known}]}
 *   POST /wifi/scan       start a scan (409 while one runs or the radio is connecting)
 *   POST /wifi/add        ssid, password, hidden=0|1, open=1 (explicitly no password)
 *   POST /wifi/remove     ssid
 *   POST /wifi/move       ssid, dir=-1|1 (towards the front = higher priority)
 *   POST /wifi/ap         ssid, password (8..63 characters)
 *   POST /wifi/reconnect  drop the current connection and run the autoconnect again
 *
 * Passwords only ever come IN, through POST bodies: a GET parameter would end up in the request
 * log on the SD card (WebInstr logs the URL). Nothing here sends a stored password back, and
 * nothing logs one.
 */

#include <WifiWebserver.h>
#include <Singletons.h>
#include <ArduinoJson.h>
#include "WebPage.h"

static const BCLogger::LogTag TAG = BCLogger::TAG_WIFI;

static const char* phaseText(WifiPhase p) {
	switch (p) {
	case WifiPhase::OFF: return "off";
	case WifiPhase::SCANNING: return "scanning";
	case WifiPhase::CONNECTING: return "connecting";
	case WifiPhase::WAITING: return "waiting";
	case WifiPhase::ONLINE: return "online";
	case WifiPhase::ACCESS_POINT: return "ap";
	}
	return "?";
}

static String param(AsyncWebServerRequest* request, const char* name) {
	return request->hasParam(name, true) ? request->getParam(name, true)->value() : String();
}

static void sendJson(AsyncWebServerRequest* request, JsonDocument& doc) {
	String out;
	serializeJson(doc, out);
	AsyncWebServerResponse* response = request->beginResponse(200, "application/json", out);
	response->addHeader("Cache-Control", "no-store");
	request->send(response);
}

static void sendResult(AsyncWebServerRequest* request, WifiCfg::Result r) {
	switch (r) {
	case WifiCfg::Result::OK:
	case WifiCfg::Result::ADDED:
	case WifiCfg::Result::UPDATED:
		request->send(200, "text/plain", r == WifiCfg::Result::ADDED ? "Network added" : "Saved");
		break;
	case WifiCfg::Result::BAD_SSID:
		request->send(400, "text/plain", "The SSID must be 1 to 32 bytes long");
		break;
	case WifiCfg::Result::BAD_PASSWORD:
		request->send(400, "text/plain", "The password must be 8 to 63 characters (empty for an open network)");
		break;
	case WifiCfg::Result::FULL:
		request->send(409, "text/plain", "The list is full - remove a network first");
		break;
	case WifiCfg::Result::BAD_INDEX:
		request->send(404, "text/plain", "No such network");
		break;
	}
}

static const char PAGE_CSS[] =
	"h3{margin:22px 0 6px}"
	".net td{padding:6px 8px}"
	".net .name{width:100%;word-break:break-all}"
	".net button{padding:4px 10px;min-width:34px}"
	".muted{color:var(--rr-muted,#9BA097)}"
	"#status{margin-bottom:6px}"
	"form label{display:block;margin-top:10px}"
	"form .row{margin-top:12px}";

static const char PAGE_BODY[] = R"HTML(
<p class="eyebrow" id="status">&nbsp;</p>

<h3>Saved networks</h3>
<p class="eyebrow">The first network in range is tried first. The order is the priority.</p>
<table class="net"><tbody id="nets"></tbody></table>
<div class="row" style="margin-top:10px;">
  <button onclick="reconnect()">Connect again</button>
</div>

<h3>Add or change a network</h3>
<form onsubmit="save();return false;" autocomplete="off">
  <label for="found">In range</label>
  <div class="row">
    <select id="found" onchange="pick()"><option value="">- scan first -</option></select>
    <button type="button" id="scanbtn" onclick="scan()">Scan</button>
  </div>
  <label for="ssid">SSID</label>
  <input type="text" id="ssid" maxlength="32" placeholder="network name">
  <label for="pw">Password</label>
  <input type="password" id="pw" maxlength="63" autocomplete="new-password" placeholder="leave empty to keep the stored one">
  <label><input type="checkbox" id="hidden"> hidden network (not found by a scan)</label>
  <label><input type="checkbox" id="open"> open network (remove the password)</label>
  <div class="row"><input type="submit" value="Save"></div>
</form>

<h3>Hotspot</h3>
<p class="eyebrow">Started from the settings screen of the device. The password is shown on its display.
It is not shown here - set a new one if you want to change it.</p>
<form onsubmit="saveAp();return false;" autocomplete="off">
  <label for="apssid">SSID</label>
  <input type="text" id="apssid" maxlength="32">
  <label for="appw">New password</label>
  <input type="password" id="appw" maxlength="63" autocomplete="new-password" placeholder="8 to 63 characters">
  <div class="row"><input type="submit" value="Save hotspot settings"></div>
</form>
)HTML";

static const char PAGE_SCRIPT[] = R"JS(
const $=id=>document.getElementById(id);
let known=[];
async function post(url,data){
  try{
    const r=await fetch(url,{method:'POST',body:new URLSearchParams(data||{})});
    const t=await r.text();
    toast(t||(r.ok?'OK':'Failed ('+r.status+')'),!r.ok);
    return r.ok;
  }catch(e){toast('Request failed',true);return false;}
}
function cell(tr,text,cls){const td=document.createElement('td');if(cls)td.className=cls;td.textContent=text;tr.appendChild(td);return td;}
function btn(td,label,title,fn,off){
  const b=document.createElement('button');b.type='button';b.textContent=label;b.title=title;
  b.disabled=!!off;b.onclick=fn;td.appendChild(b);
}
async function load(){
  try{
    const s=await (await fetch('/wifi/state',{cache:'no-store'})).json();
    let t={online:'Connected to '+s.ssid+' ('+s.ip+', '+s.rssi+' dBm)',ap:'Hotspot '+s.ssid+' ('+s.ip+', '+s.stations+' client(s))',
      scanning:'Looking for a saved network ...',connecting:'Connecting to '+s.ssid+' ...',waiting:'No saved network in range, trying again ...',off:'WiFi is off'}[s.phase]||s.phase;
    $('status').textContent=t;
    known=s.networks;
    const body=$('nets');body.textContent='';
    s.networks.forEach((n,i)=>{
      const tr=document.createElement('tr');
      cell(tr,(i+1)+'.','muted');
      cell(tr,n.ssid+(n.open?' (open)':'')+(n.hidden?' (hidden)':''),'name');
      const td=document.createElement('td');td.style.whiteSpace='nowrap';
      btn(td,'▲','Higher priority',()=>move(n.ssid,-1),i===0);
      btn(td,'▼','Lower priority',()=>move(n.ssid,1),i===s.networks.length-1);
      btn(td,'Edit','Change password',()=>edit(n));
      btn(td,'Delete','Remove',()=>del(n.ssid));
      tr.appendChild(td);body.appendChild(tr);
    });
    if(!s.networks.length){const tr=document.createElement('tr');cell(tr,'No network saved yet.','muted');body.appendChild(tr);}
    if(document.activeElement!==$('apssid')&&!$('apssid').value)$('apssid').value=s.apSsid;
  }catch(e){$('status').textContent='No answer from the device';}
}
function edit(n){$('ssid').value=n.ssid;$('hidden').checked=n.hidden;$('open').checked=false;$('pw').value='';$('pw').focus();}
async function move(ssid,dir){if(await post('/wifi/move',{ssid:ssid,dir:dir}))load();}
async function del(ssid){if(confirm('Remove "'+ssid+'"?')&&await post('/wifi/remove',{ssid:ssid}))load();}
async function save(){
  const ssid=$('ssid').value;
  if(!ssid){toast('Enter or pick an SSID',true);return;}
  if(await post('/wifi/add',{ssid:ssid,password:$('pw').value,hidden:$('hidden').checked?1:0,open:$('open').checked?1:0})){
    $('pw').value='';$('open').checked=false;load();
  }
}
async function saveAp(){if(await post('/wifi/ap',{ssid:$('apssid').value,password:$('appw').value}))$('appw').value='';}
async function reconnect(){
  if(!confirm('Drop the current connection and look for a saved network again? This page may stop answering.'))return;
  post('/wifi/reconnect');
}
function pick(){const v=$('found').value;if(v){$('ssid').value=v;$('hidden').checked=false;$('pw').focus();}}
async function scan(){
  $('scanbtn').disabled=true;$('found').options[0].textContent='scanning ...';
  const ok=await post('/wifi/scan');
  for(let i=0;ok&&i<20;i++){
    await new Promise(r=>setTimeout(r,1000));
    try{
      const s=await (await fetch('/wifi/scan',{cache:'no-store'})).json();
      if(s.scanning)continue;
      const f=$('found');f.textContent='';
      const o0=document.createElement('option');o0.value='';o0.textContent=s.results.length?'- pick one -':'- nothing found -';f.appendChild(o0);
      s.results.forEach(r=>{const o=document.createElement('option');o.value=r.ssid;
        o.textContent=r.ssid+'  ('+r.rssi+' dBm'+(r.open?', open':'')+(r.known?', saved':'')+')';f.appendChild(o);});
      break;
    }catch(e){}
  }
  $('scanbtn').disabled=false;
}
load();setInterval(load,5000);
)JS";

void WifiWebserver::setupWifiRoutes() {
	server.on("/wifi/state", HTTP_GET, [this](AsyncWebServerRequest* request) {
		WifiStatus st;
		getStatus(st);
		JsonDocument doc;
		doc["phase"] = phaseText(st.phase);
		doc["ssid"] = st.ssid;
		doc["ip"] = st.ip;
		doc["rssi"] = st.rssi;
		doc["stations"] = st.stations;
		doc["scanning"] = st.scanning;
		doc["max"] = WifiCfg::MAX_NETWORKS;
		JsonArray nets = doc["networks"].to<JsonArray>();
		{
			xSemaphoreTake(cfgMutex, portMAX_DELAY);
			for (size_t i = 0; i < cfg.count(); i++) {
				JsonObject n = nets.add<JsonObject>();
				n["ssid"] = cfg.ssid(i);
				n["open"] = !cfg.hasPassword(i);
				n["hidden"] = cfg.hidden(i);
			}
			doc["apSsid"] = cfg.accessPoint().ssid;
			xSemaphoreGive(cfgMutex);
		}
		sendJson(request, doc);
	});

	server.on("/wifi/scan", HTTP_GET, [this](AsyncWebServerRequest* request) {
		WifiScanEntry found[SCAN_MAX];		// ~0.8 KB of the async_tcp stack
		WifiStatus st;
		getStatus(st);
		const size_t n = getScan(found, SCAN_MAX);
		JsonDocument doc;
		doc["scanning"] = st.scanning;
		doc["version"] = st.scanVersion;
		JsonArray results = doc["results"].to<JsonArray>();
		for (size_t i = 0; i < n; i++) {
			JsonObject r = results.add<JsonObject>();
			r["ssid"] = found[i].ssid;
			r["rssi"] = found[i].rssi;
			r["open"] = found[i].open;
			r["known"] = found[i].known;
		}
		sendJson(request, doc);
	});

	server.on("/wifi/scan", HTTP_POST, [this](AsyncWebServerRequest* request) {
		if (requestScan()) {
			request->send(200, "text/plain", "Scanning ...");
		} else {
			request->send(409, "text/plain", "Busy - try again in a moment");
		}
	});

	server.on("/wifi/add", HTTP_POST, [this](AsyncWebServerRequest* request) {
		const String ssid = param(request, "ssid");
		const String pw = param(request, "password");
		const bool hidden = param(request, "hidden") == "1";
		const bool open = param(request, "open") == "1";
		// An empty password for a network that is already stored keeps the old one, unless
		// "open" says it is meant (so the hidden flag can be changed without typing it again).
		sendResult(request, addNetwork(ssid.c_str(), pw.c_str(), hidden, !open));
	});

	server.on("/wifi/remove", HTTP_POST, [this](AsyncWebServerRequest* request) {
		sendResult(request, removeNetwork(param(request, "ssid").c_str()));
	});

	server.on("/wifi/move", HTTP_POST, [this](AsyncWebServerRequest* request) {
		const int dir = param(request, "dir").toInt();
		sendResult(request, moveNetwork(param(request, "ssid").c_str(), dir < 0 ? -1 : 1));
	});

	server.on("/wifi/ap", HTTP_POST, [this](AsyncWebServerRequest* request) {
		sendResult(request, setAccessPoint(param(request, "ssid").c_str(), param(request, "password").c_str()));
	});

	server.on("/wifi/reconnect", HTTP_POST, [this](AsyncWebServerRequest* request) {
		requestRestart();
		request->send(200, "text/plain", "Looking for a network ...");
	});

	// Registered last: a prefix route "/wifi" also matches "/wifi/..." for the same method,
	// and the first matching handler wins.
	server.on("/wifi", HTTP_GET, [](AsyncWebServerRequest* request) {
		String html;
		html.reserve(8192);
		WebPage::begin(html, "WiFi", PAGE_CSS);
		html += PAGE_BODY;
		WebPage::end(html, PAGE_SCRIPT);
		request->send(200, "text/html", html);
	});
}

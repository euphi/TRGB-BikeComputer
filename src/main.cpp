#include <Arduino.h>

#include "Singletons.h"
#include "BootLogoRimRidge.h"
#include "WebInstrument.h"
#include "CrashInfo.h"
#include "NvsUtil.h"
#include "I2CBus.h"

#include <WiFi.h>
#include <Battery.h>

// Measured discharge curve instead of the library's generic sigmoid: the percentage is the
// share of the runtime that is left, so 50% really means half the time to switch-off. Taken
// from the full run of 2026-10-13 22:00 to 2026-10-14 06:19 (8 h 19 min, display on, WiFi on
// for the first 1.5 h, no sensors -- the minute lines of logBatteryState() in
// Tools/TestData/battery-discharge-20261013.csv; monotone least-squares fit of voltage over
// time, percentage = remaining time / total time; the breakpoints keep that within 2.2
// percentage points). Caveats worth knowing before trusting the low end:
//  * The bounds are what the ADC reads here, not cell voltage: a full battery reads about
//    3.9 V right after unplugging (the charger pulls the pin to 4.5 V, see `charging` below),
//    and the board cut off at 2.65 V. Both are well below the 4.2 V / 3.0 V of a LiPo, so
//    either the divider or the measuring point has an offset -- for the percentage it does
//    not matter, as long as the same pin is read.
//  * The reading sags under load: switching WiFi off lifted it by about 25 mV, so on a ride
//    (sensors, more draw) the percentage reads somewhat low. Only one run, at room
//    temperature; cold weather will shorten it.
static uint8_t batteryCurve(uint16_t mv, uint16_t, uint16_t) {
	static const struct { uint16_t mv; uint8_t pct; } CURVE[] = {	// percent, minutes left
		{3910, 100},	// 499
		{3860, 100},	// 499
		{3760,  88},	// 439
		{3710,  85},	// 424
		{3660,  75},	// 374
		{3540,  57},	// 284
		{3520,  51},	// 254
		{3480,  34},	// 170
		{3400,  22},	// 110
		{3100,   9},	//  45
		{2650,   0},	//   0
	};
	for (size_t i = 1; i < sizeof(CURVE) / sizeof(CURVE[0]); i++) {		// Battery::level() clamps outside
		if (mv >= CURVE[i].mv) {
			const uint16_t span = CURVE[i - 1].mv - CURVE[i].mv;
			return CURVE[i].pct + (CURVE[i - 1].pct - CURVE[i].pct) * (mv - CURVE[i].mv) / span;
		}
	}
	return 0;
}
Battery batt = Battery(2650, 3910, BAT_VOLT_PIN);	// bounds of batteryCurve(), see there

#include <Ticker.h>
Ticker batCheckTicker;

void errorCallback(cmd_error* e) {
    CommandError cmdError(e);
    bclog.log(BCLogger::Log_Warn, BCLogger::TAG_CLI, "Serial command "+ cmdError.toString());
    if (cmdError.hasCommand()) {
        Serial.print("Wrong parameters. Help: ");
        Serial.println(cmdError.getCommand().toString());
    } else {
        Serial.println("Unknown command -- \"help\" lists them, Tab completes.");
    }
}

Command cmdPing;
Command cmdBat;
Command cmdMem;

int8_t batLevel = -1;

// Once a minute at Info under TAG_STAT (the file level of TAG_OP is Error), so the discharge
// curve ends up in the debug log together with what drained it: WiFi and connected sensors.
static void logBatteryState(uint16_t mv, uint8_t perc, bool charging) {
	const char* wifi = WiFi.getMode() == WIFI_OFF ? "off" : WiFi.getMode() == WIFI_AP ? "ap"
	                 : WiFi.status() == WL_CONNECTED ? "sta" : "search";
	char ble[BLEDevices::DEV_COUNT + 1];
	uint8_t connected = 0;
	for (int d = 0; d < BLEDevices::DEV_COUNT; d++) {
		const bool c = bleDevs.getDevStatus(BLEDevices::EDevType(d)).state == BLEDevices::CONN_CONNECTED;
		ble[d] = c ? "HcCFN"[d] : '-';		// HRM, CSC 1, CSC 2, FL, Nav
		connected += c;
	}
	ble[BLEDevices::DEV_COUNT] = 0;
	bclog.logf(BCLogger::Log_Info, BCLogger::TAG_STAT, "🔋 Battery %u mV (avg 60 s) %u%% %s | wifi %s | BLE %u [%s] | heap %u",
	           mv, perc, charging ? "charging" : "discharging", wifi, connected, ble, (unsigned) ESP.getFreeHeap());
}

void batCheck() {
	const uint16_t mv = batt.voltage();
	const bool charging = mv > 4150;
	bclog.logf(BCLogger::Log_Debug, BCLogger::TAG_OP, "Battery:  %d%% [%d mV]- charging [%c]", batt.level(), mv, charging ? 'x' : ' ');
	ui.updateBatInt(mv / 1000.0, batt.level(uint16_t (ui.getBatIntVoltageAvg()*1000)), charging);

	static uint8_t seconds = 0;
	if (++seconds >= 60) {
		seconds = 0;
		const float avg = ui.getBatIntVoltageAvg();		// NAN until the first minute is full
		const uint16_t avgMv = isnan(avg) ? mv : uint16_t(avg * 1000);
		logBatteryState(avgMv, batt.level(avgMv), charging);
	}
}

void setup() {
	trgb.setLogo(bootLogoRimRidge);
	trgb.init();
	if (!I2CBus::guardTouch()) Serial.println("No touch device found to put the I2C lock around");
	console.setup();
	webserver.setup(); // start early to update system time as soon as possible
	TRGBSuppport::print_chip_info();
	TRGBSuppport::scan_iic();
	trgb.SD_init();
	batt.begin(3300, 2.0/4.0 * 1.03, &batteryCurve); // divider ratio is 2, but ESP32-S3 has 4096 bit DAC instead of 1024, so an additional division by 4 is needed --> 0.5 ratio  + 3% error correction (individual setting?)

	bclog.setup();
	NvsUtil::removeStaleKeys();
	CrashInfo::logBoot();
    cli.setOnError(errorCallback);
    cmdPing = console.addCmd("ping", [](cmd* c) {Serial.println("Pong!");});
    cmdPing.setDescription("Responds with a pong and logs it");
    cmdBat = console.addCmd("showbat", [](cmd* c) {const uint16_t mv = batt.voltage();
    	Serial.printf("Battery: %d%% [%d mV] - charging [%c]\r\n", batt.level(mv), mv, mv > 4150 ? 'x' : ' ');});	// 4150: same threshold as batCheck()
    // Same output FlusherTask emits every 5s, but on demand -- so a memory reading right
    // after a request doesn't have to wait for the next flush cycle. Stack watermarks
    // included here (the periodic report only prints them every 60s).
    cmdMem = console.addCmd("mem", [](cmd* c) {WebInstr::report(true, true); WebInstr::reportDisplayBuffers(); WebInstr::drain();});
    cmdMem.setDescription("Log free internal/DMA/PSRAM heap, open requests and task stack watermarks");
	ui.initDisplay();
    stats.setup();
	climb.setup();
	routeMon.setup();
	bleDevs.setup();
#ifdef BC_SIM
	sim.setup();
#endif
#ifdef TRGBBC_SENSORS_I2C
	sensors.setup();
#endif
	batCheckTicker.attach(1, batCheck);
	// Last: every module above has registered its web routes by now, so the server may
	// start serving. Until this point checkLoop() holds it back even if WiFi is already up.
	webserver.enableWebserver();
}

void loop() {
	console.poll();		// command line
}

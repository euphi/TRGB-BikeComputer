#include <Arduino.h>

#include "Singletons.h"
#include "BootLogoRimRidge.h"
#include "WebInstrument.h"
#include "CrashInfo.h"
#include "NvsUtil.h"
#include "I2CBus.h"

#include <WiFi.h>
#include <Battery.h>
#include <esp_system.h>

#include "BatteryGauge.h"

// The Battery library only reads the pin here; the percentage comes from BatGauge::Gauge, which
// runs a clock and lets the measured discharge curve correct it. src/BatteryGauge.h explains why
// the voltage alone cannot do that job in the middle of the discharge. batt.level() still
// answers with the bare curve, so `showbat` can print both -- that is what tuning needs.
static uint8_t bareCurve(uint16_t mv, uint16_t, uint16_t) { return BatGauge::curvePercent(mv); }
Battery batt = Battery(BatGauge::EMPTY_MV, BatGauge::FULL_MV, BAT_VOLT_PIN);
BatGauge::Gauge batGauge;

// In RTC memory, not in the NVS: the estimate has to survive a brownout reboot or a restart
// from the settings screen -- otherwise the gauge re-anchors to the plateau voltage, the one
// reading it exists to avoid -- but writing it every minute would stall the bus the panel reads
// its framebuffer from (doc/PITFALLS.md, "NVS-Schreiben und Display-Flackern"). RTC memory
// survives a reset and deep sleep but not a power cycle, which is exactly the right rule: after
// a power-on the board was off, probably on the charger, and nothing remembered is worth having.
RTC_NOINIT_ATTR static BatGauge::State batRtcState;
RTC_NOINIT_ATTR static uint32_t batRtcMagic;
RTC_NOINIT_ATTR static uint32_t batRtcEpoch;	// unix time of the last save, 0 = clock was not set
static constexpr uint32_t BAT_RTC_MAGIC = 0x62617467;	// "batg"
// A gap this long makes the remembered estimate worthless: deep sleep keeps drawing current
// while the clock inside the gauge stands still, and nobody has measured the sleep current to
// drain it with. After a rest the voltage is at its best anyway -- an unloaded cell, no sag --
// so the gauge starts over from the curve. Short gaps (a reboot, a brief sleep) are kept.
static constexpr uint32_t BAT_STALE_S = 600;

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

// Once a minute at Info under TAG_STAT (the file level of TAG_OP is Error), so the discharge
// curve ends up in the debug log together with what drained it: WiFi and connected sensors.
static void logBatteryState(uint16_t mv, uint8_t perc, uint16_t remainMin, bool charging) {
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
	// The estimated minutes left are what a ride has to say about Config::fullRuntimeMin -- the
	// one number of the gauge that the lab runs cannot pin down. Meaningless on the charger.
	char left[16] = "";
	if (!charging) snprintf(left, sizeof(left), " (~%u min)", remainMin);
	bclog.logf(BCLogger::Log_Info, BCLogger::TAG_STAT, "🔋 Battery %u mV (avg 60 s) %u%%%s %s | wifi %s | BLE %u [%s] | heap %u",
	           mv, perc, left, charging ? "charging" : "discharging", wifi, connected, ble, (unsigned) ESP.getFreeHeap());
}

void batCheck() {
	const uint16_t mv = batt.voltage();
	const bool charging = mv > 4150;
	const float avg = ui.getBatIntVoltageAvg();		// NAN until the first minute is full
	const uint16_t avgMv = isnan(avg) ? mv : uint16_t(avg * 1000);
	batGauge.update(1000, avgMv, charging);			// the ticker calls us once a second
	batRtcState = batGauge.state();					// survives a reset, see BAT_RTC_MAGIC
	batRtcEpoch = (uint32_t) time(nullptr);			// ... but only counts while the clock is set
	const uint8_t perc = batGauge.percent();
	bclog.logf(BCLogger::Log_Debug, BCLogger::TAG_OP, "Battery:  %u%% [%d mV]- charging [%c]", perc, mv, charging ? 'x' : ' ');
	ui.updateBatInt(mv / 1000.0, perc, charging, batGauge.remainingMinutes());

	static uint8_t seconds = 0;
	if (++seconds >= 60) {
		seconds = 0;
		logBatteryState(avgMv, perc, batGauge.remainingMinutes(), charging);
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
	batt.begin(3300, 2.0/4.0 * 1.03, &bareCurve); // divider ratio is 2, but ESP32-S3 has 4096 bit DAC instead of 1024, so an additional division by 4 is needed --> 0.5 ratio  + 3% error correction (individual setting?)

	// Anything but a power-on means the board kept running on the same charge, so the estimate
	// from before the reset beats what the voltage alone can say now -- unless too much time has
	// passed since it was written, see BAT_STALE_S. Without a set clock the gap is unknown, and a
	// reset without one is a reboot, not a long sleep.
	const uint32_t batNow = (uint32_t) time(nullptr);
	const bool batGapKnown = batRtcEpoch > 1600000000 && batNow > batRtcEpoch;
	if (esp_reset_reason() != ESP_RST_POWERON && batRtcMagic == BAT_RTC_MAGIC
	    && !(batGapKnown && batNow - batRtcEpoch > BAT_STALE_S)) batGauge.restore(batRtcState);
	batRtcMagic = BAT_RTC_MAGIC;

	bclog.setup();
	NvsUtil::removeStaleKeys();
	CrashInfo::logBoot();
    cli.setOnError(errorCallback);
    cmdPing = console.addCmd("ping", [](cmd* c) {Serial.println("Pong!");});
    cmdPing.setDescription("Responds with a pong and logs it");
    cmdBat = console.addCmd("showbat", [](cmd* c) {const uint16_t mv = batt.voltage();
    	Serial.printf("Battery: %u%% (~%u min) [%d mV] - charging [%c] - bare curve %u%%\r\n",
    	              batGauge.percent(), batGauge.remainingMinutes(), mv, mv > 4150 ? 'x' : ' ',
    	              BatGauge::curvePercent(mv));});	// 4150: same threshold as batCheck()
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

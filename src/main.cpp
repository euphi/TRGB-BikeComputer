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
// its framebuffer from (doc/PITFALLS.md, "NVS-Schreiben und Display-Flackern").
RTC_NOINIT_ATTR static BatGauge::State batRtcState;
RTC_NOINIT_ATTR static uint32_t batRtcMagic;
static constexpr uint32_t BAT_RTC_MAGIC = 0x6261746A;	// "batj" -- bump when State changes layout

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
	char wifiText[24];
	snprintf(wifiText, sizeof(wifiText), "%s", wifi);
	if (WiFi.getMode() != WIFI_OFF && WiFi.status() == WL_CONNECTED) snprintf(wifiText, sizeof(wifiText), "%s %d dBm", wifi, (int) WiFi.RSSI());
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
	           mv, perc, left, charging ? "charging" : "discharging", wifiText, connected, ble, (unsigned) ESP.getFreeHeap());
}

void batCheck() {
	const uint16_t mv = batt.voltage();
	const bool charging = mv > 4150;
	const float avg = ui.getBatIntVoltageAvg();		// NAN until the first minute is full
	const uint16_t avgMv = isnan(avg) ? mv : uint16_t(avg * 1000);
	batGauge.update(1000, avgMv, charging);			// the ticker calls us once a second
	batRtcState = batGauge.state();					// survives a reset, see BAT_RTC_MAGIC
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

	// Only a reset in the middle of running keeps the estimate: the board went on drawing from
	// the same charge, so what was counted beats a single voltage reading. After deep sleep or a
	// power-on it is the other way round -- that is how the BC is normally charged, so the battery
	// may be anything from where it was to full, and the rested voltage is the best there is.
	switch (esp_reset_reason()) {
		case ESP_RST_BROWNOUT: case ESP_RST_PANIC: case ESP_RST_INT_WDT: case ESP_RST_TASK_WDT:
		case ESP_RST_WDT: case ESP_RST_SW:
			if (batRtcMagic == BAT_RTC_MAGIC) batGauge.restore(batRtcState);
			break;
		default:		// ESP_RST_DEEPSLEEP, ESP_RST_POWERON, ...: anchor to the voltage
			break;
	}
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
	logPush.setup();
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

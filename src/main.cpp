#include <Arduino.h>

#include "Singletons.h"
#include "BootLogoRimRidge.h"
#include "WebInstrument.h"
#include "CrashInfo.h"
#include "NvsUtil.h"
#include "I2CBus.h"

#include <Battery.h>
Battery batt = Battery(3000, 4200, BAT_VOLT_PIN);

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

void batCheck() {
	bclog.logf(BCLogger::Log_Debug, BCLogger::TAG_OP, "Battery:  %d%% [%d mV]- charging [%c]", batt.level(), batt.voltage(), batt.voltage() > 4150 ? 'x' : ' ');
	ui.updateBatInt(batt.voltage() / 1000.0, batt.level(uint16_t (ui.getBatIntVoltageAvg()*1000)), batt.voltage() > 4150);
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
	batt.begin(3300, 2.0/4.0 * 1.03, &sigmoidal); // divider ratio is 2, but ESP32-S3 has 4096 bit DAC instead of 1024, so an additional division by 4 is needed --> 0.5 ratio  + 3% error correction (individual setting?)

	bclog.setup();
	NvsUtil::removeStaleKeys();
	CrashInfo::logBoot();
    cli.setOnError(errorCallback);
    cmdPing = console.addCmd("ping", [](cmd* c) {Serial.println("Pong!");});
    cmdPing.setDescription("Responds with a pong and logs it");
    cmdBat = console.addCmd("showbat", [](cmd* c) {Serial.printf("Battery: %d%% - charging [%c]\r\n", batt.level(), batt.voltage()>3300?'x':' ');});
    // Same output FlusherTask emits every 5s, but on demand -- so a memory reading right
    // after a request doesn't have to wait for the next flush cycle. Stack watermarks
    // included here (the periodic report only prints them every 60s).
    cmdMem = console.addCmd("mem", [](cmd* c) {WebInstr::report(true, true); WebInstr::reportDisplayBuffers(); WebInstr::drain();});
    cmdMem.setDescription("Log free internal/DMA/PSRAM heap, open requests and task stack watermarks");
	ui.initDisplay();
    stats.setup();
	climb.setup();
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

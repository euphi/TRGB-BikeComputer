#include <Arduino.h>

#include "Singletons.h"

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
    }
}

Command cmdPing;
Command cmdBat;
String inputBuffer;

int8_t batLevel = -1;

void batCheck() {
	bclog.logf(BCLogger::Log_Debug, BCLogger::TAG_OP, "Battery:  %d%% [%d mV]- charging [%c]", batt.level(), batt.voltage(), batt.voltage() > 4150 ? 'x' : ' ');
	ui.updateBatInt(batt.voltage() / 1000.0, batt.level(uint16_t (ui.getBatIntVoltageAvg()*1000)), batt.voltage() > 4150);
	sensors.readBMI160();  // FIXME read sensor in another functions
}

void setup() {
	trgb.init();
	webserver.setup(); // start early to update system time as soon as possible
	TRGBSuppport::print_chip_info();
	TRGBSuppport::scan_iic();
	trgb.SD_init();
	batt.begin(3300, 2.0/4.0 * 1.03, &sigmoidal); // divider ratio is 2, but ESP32-S3 has 4096 bit DAC instead of 1024, so an additional division by 4 is needed --> 0.5 ratio  + 3% error correction (individual setting?)

	bclog.setup();
    cli.setOnError(errorCallback);
    cli.available();
    cmdPing = cli.addCmd("ping", [](cmd* c) {Serial.println("Pong!");});
    cmdPing.setDescription("Responds with a pong and logs it");
    cmdBat = cli.addCmd("showbat", [](cmd* c) {Serial.printf("Battery: %d%% - charging [%c]", batt.level(), batt.voltage()>3300?'x':' ');});
	ui.initDisplay();
    stats.setup();
	bleDevs.setup();
	sensors.setup();
	batCheckTicker.attach(1, batCheck);

}

void loop() {
    // Handle command line
	if (Serial.available()) {
    	bool lineComplete = false;
    	while (Serial.available()) {
    		char inChar = Serial.read();
    		if (inChar == 255) continue;
    		if (inChar == '\n') lineComplete = true;
    		inputBuffer += inChar;
    		Serial.print(inChar);		// Echo
    	}
    	if (lineComplete) {
    		bclog.log(BCLogger::Log_Info, BCLogger::TAG_OP, "Received command: " + inputBuffer);
    	    cli.parse(inputBuffer);
    	    inputBuffer.clear();
    	}
    }
}

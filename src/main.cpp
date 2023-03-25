#include <Arduino.h>

#include "Singletons.h"

#include <WifiWebserver.h>
WifiWebserver webserver;

#include "BLEDevices.h"
BLEDevices bleDevs;

void errorCallback(cmd_error* e) {
    CommandError cmdError(e);
    bclog.log(BCLogger::Log_Warn, BCLogger::TAG_CLI, "Serial command "+ cmdError.toString());
    if (cmdError.hasCommand()) {
        Serial.print("Wrong parameters. Help: ");
        Serial.println(cmdError.getCommand().toString());
    }
}

Command cmdPing;
String inputBuffer;

void setup() {
	trgb.init();
	webserver.setup(); // start early to update system time as soon as possible
	TRGBSuppport::print_chip_info();
	//TRGBSuppport::scan_iic();
	trgb.SD_init();
	bclog.setup();
    cli.setOnError(errorCallback);
    cli.available();
    cmdPing = cli.addCmd("ping", [](cmd* c) {Serial.println("Pong!");});
    cmdPing.setDescription("Responds with a pong and logs it");
	ui.initDisplay();
    stats.setup();
	bleDevs.setup();
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

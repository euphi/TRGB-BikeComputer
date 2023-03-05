#include <Arduino.h>

#include "Singletons.h"

#include <WifiWebserver.h>
WifiWebserver webserver;

#include "TRGBSuppport.h"
TRGBSuppport trgb;

#include "BLEDevices.h"
BLEDevices bleDevs;


#include <Ticker.h>
Ticker tickerDispUpdate;		// Ticker  for slow display updates. Most data should be updated directly, so this can run not that often


Command cmdPing;
String inputBuffer;
void errorCallback(cmd_error* e) {
    CommandError cmdError(e); // Create wrapper object
    bclog.log(BCLogger::Log_Warn, BCLogger::TAG_CLI, "Serial command "+ cmdError.toString());
    if (cmdError.hasCommand()) {
        Serial.print("Wrong parameters. Help: ");
        Serial.println(cmdError.getCommand().toString());
    }
}

void pingCallback(cmd* c) {
    Command cmd(c); // Create wrapper object
    Serial.println("Pong!");
}

void displayUpdate() {  // Slow running loop function (4x per second) to update display information
	// Note: Try to update values directly when new information arrives, e.g. write speed from sensor directly to UI via UIFacade. Use this function here only for global stuff.
}

void setup() {
	trgb.init();
	webserver.setup(); // start early to update system time as soon as possible
	TRGBSuppport::print_chip_info();
	//TRGBSuppport::scan_iic();
	trgb.SD_init();
	bclog.setup();
    cli.setOnError(errorCallback);
    cmdPing = cli.addCmd("ping", pingCallback);
    cmdPing.setDescription("Responds with a pong and logs it");
	ui.initDisplay();

    stats.setup();
	tickerDispUpdate.attach_ms(250, displayUpdate);
	bleDevs.setup();
	//bclog.replayFile("/BIKECOMP/20230304/N_144428.log");

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
    		Serial.print(inChar);
    	}
    	if (lineComplete) {
    		bclog.log(BCLogger::Log_Info, BCLogger::TAG_OP, "Received command: " + inputBuffer);
    	    cli.parse(inputBuffer);
    	    inputBuffer.clear();
    	}
    }
}

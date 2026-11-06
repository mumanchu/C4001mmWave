///////////////////////////////////////////////////////////////////////////////
// Example Sketch for C4001mmWave Library
// for DFRobot C4001 mmWave sensors, SKU SEN0609 & SEN0610, UART interface
// Copyright (C) muman.ch and mumanchu, 2026.11.06
// 
// https://github.com/mumanchu/C4001mmWave

#define DEBUG

#include "C4001mmWave.h"
C4001mmWave c4001;

#ifdef ARDUINO_ARCH_STM32
// On the STM32F103 Nucleo board, 'Serial' is the USB port
// so we use USART1, TX=D8 (PA9), RX=D2 (PA10) for the mmWave sensor
Uart Serial1(USART1);
#endif

// Change the C4001 baud rate, 4800..115200
bool changeBaudRate(ulong newBaudRate)
{
	// must be in Stop mode to receive commands
	if (!c4001.sensorStop()) {
		C4001_LOGERROR("sensorStop() failed");
		return false;
	}

	// set the baud rate, this also checks for valid baud rates
	if (!c4001.setUart(newBaudRate)) {
		C4001_LOGERROR("setUart() failed, bad baud rate?");
		return false;
	}

	// save new baud rate to flash
	if (!c4001.saveConfig()) {
		C4001_LOGERROR("saveConfig() failed");
		return false;
	}

	// the response returned at the new baud rate! 
	// so this always times out
	c4001.resetSystem();
	delay(2000);

	// continue at the new baud rate
	Serial1.begin(newBaudRate);

	// it must be started at the new baud rate
	return c4001.sensorStart();
}

// Fatal error occurred
void fatal(const char* msg)
{
	Serial.println(msg);
	Serial.flush();
	while (1) 
		yield();
}

// Get ready...
void setup()
{
	// I use different pins for Serial logging
	// this is only for the STM32 Nucleo-64 boards
	// else Serial is the USB which is also used for download
	#ifdef ARDUINO_NUCLEO_64
	Serial.setTx(PC_10);
	Serial.setRx(PC_11);
	#endif

	Serial.begin(115200);
	delay(1000);

	// PuTTY clear screen and scrollback
	// patch out if not using PuTTY
	Serial.print("\033[2J\033[H\033[3J");

	Serial.println("\r\nSTM32 Started...\r\n");
	Serial.flush();

	// SEN0609 (25m) uart at 9600 baud by default
	Serial1.begin(9600);

	// the sensor is in Stop after this, ready to receive commands 
	if (!c4001.begin(&Serial1))
		fatal("begin() failed");

	// display some information
	char hwVersion[32];
	c4001.getHWV(hwVersion);
	char swVersion[32];
	c4001.getSWV(swVersion);
	ulong baudRate;
	c4001.getUart(&baudRate);

	// enable this to permanently change the baud rate, e.g. to 19200
	// update the baud rate in the Serial1.begin() call above for the next run
	#if 0
	if (!changeBaudRate(19200))
		fatal("changeBaudRate() failed");
	#endif

	// configure the sensor here

	#if 1

	// Motion detection mode
	/*
	// in case you want to read the configuration
	bool microMotionEnabled;
	c4001.getMicroMotion(&microMotionEnabled);
	uint thresholdFactor;
	c4001.getThresholdFactor(&thresholdFactor);
	*/
	// set default values
	c4001.setMicroMotion(false);
	c4001.setThresholdFactor(5);

	if (!c4001.setRunApp(c4001.MotionDetectionMode))
		fatal("retRunApp() failed");

	#else

	// Presence detection mode
	/*
	// in case you want to read the configuration
	float trigRange;
	c4001.getTrigRange(&trigRange);
	uint trigSens, holdSens;
	c4001.getSensitivity(&trigSens, &holdSens);
	float confirmationLatency, disappearanceLatency;
	c4001.getLatency(&confirmationLatency, &disappearanceLatency);
	float inhibitSecs;
	c4001.getInhibit(&inhibitSecs);
	*/
	// set default values
	c4001.setTrigRange(6.0f);
	c4001.setSensitivity(5, 7);
	c4001.setLatency(0.05f, 15.0f);
	c4001.setInhibit(1.0f);

	if (!c4001.setRunApp(c4001.PresenceDetectionMode))
		fatal("retRunApp() failed");

	#endif

	// could save the configuration to flash 
	// so we don't have to configure it again
	#if 0
	c4001.sensorStop();
	if (!c4001.saveConfig()) {
		//could be "no parameter has changed", that's not an error
		//fatal("saveConfig failed");
	}
	#endif

	// start active reporting mode
	c4001.sensorStart();
}

// Go!
void loop() 
{
	// active reporting message received?
	if (c4001.receive(NULL)) {

		// Motion Detection
		if (strncmp(c4001.getRxBuf(), "$DFDMD", 6) == 0) {
			uint numTargets;	// always 1?
			float targetDistance, targetSpeed, targetEnergy;
			c4001.parseDFDMD(&numTargets, &targetDistance, &targetSpeed, &targetEnergy);
			//TODO add your own presence detection code here
		}

		// Presence Detection
		else if (strncmp(c4001.getRxBuf(), "$DFDPD", 6) == 0) {
			bool presenceDetected;
			c4001.parseDFHPD(&presenceDetected);
		}
	}

	// simple schedulers (not used)
	ulong ms = millis();

	// 500mS scheduler
	static ulong t1 = 0;
	if (t1 == 0)	// wait 500mS before 1st call
		t1 = ms;
	if (ms - t1 >= 500) {
		t1 = ms;
		//TODO do something every 500mS
	}

	// 1000mS scheduler
	static ulong t2 = 0;
	//if (t2 == 0)	// patch out to do first call immediately
	//	t2 = ms;
	if (ms - t2 >= 1000) {
		t2 = ms;
		//TODO do something every second
	}

}

///////////////////////////////////////////////////////////////////////////////
// Example Sketch for C4001mmWave Library
// for DFRobot C4001 mmWave sensors, SKU SEN0609 & SEN0610, UART interface
// 
// Copyright (C) https://muman.ch + https://github.com/mumanchu, 2026.11.09
// If you re-use this software, please include the above copyright notice
// 
// For full details see
// https://github.com/mumanchu/C4001mmWave


// Comment this out after testing to remove the logging and asserts
#define DEBUG

#include "C4001mmWave.h"
C4001mmWave c4001;


// 'Serial1' is used for the mmWave sensor's UART
// 'Serial' is for the Serial Monitor so you can see the data

#if defined(ARDUINO_SAM_DUE)
// The Arduino Due has Serial1 on TX1 (18) and RX1 (19), connect these 
// to RX and TX on the sensor
// !!!NOTE!!!  
// If you select "No Optimisation" for the Arduino Due, then the build 
// may fail with this message:
// "HardwareSerial.h:26: undefined reference to vtable for HardwareSerial"
// It's a problem with the boards package, you can find the fix here:
// https://learn.adafruit.com/proper-step-debugging-atsamd21-arduino-zero-m0/paths-and-optimizations#fixing-some-core-files-2469468-17
#endif

#if defined(ARDUINO_ARCH_STM32)
// On the STM32F103 Nucleo board, 'Serial' is the USB port
// so we use USART1, TX=D8 (PA9), RX=D2 (PA10) for the mmWave sensor
Uart Serial1(USART1);
#endif

#if defined(ARDUINO_ARCH_AVR) // || defined(ESP8266)
// Use SoftwareSerial, RX=D6, TX=D7
#include <SoftwareSerial.h>
SoftwareSerial Serial1(6 /*rxPin*/, 7 /*txPin*/);
#endif

#if defined(ARDUINO_SAMD_ZERO)
// Arduino Zero has 'Serial1' on pin 0 (RX) and pin 1 (TX), so you don't need 
// to do anything. 'Serial' is the USB port, so connect your Serial Monitor to 
// the port called 'Atmel Corp. EDBG CMSIS-DAP'. 
#endif

#if defined(ARDUINO_ARCH_ESP32)
// For the ESP32s, Serial1's pins can be defined
// see the code in setup()
#endif


// Change the C4001 baud rate, 4800..115200
bool changeBaudRate(ulong newBaudRate)
{
	// must be in Stop mode to receive commands
	if (!c4001.sensorStop()) {
		C4001_LOGERROR("sensorStop() failed");
		return false;
	}

	// set the baud rate, this also checks for valid standard baud rates
	if (!c4001.setUart(newBaudRate)) {
		C4001_LOGERROR("setUart() failed, bad baud rate?");
		return false;
	}

	// save new baud rate to flash
	if (!c4001.saveConfig()) {
		C4001_LOGERROR("saveConfig() failed");
		return false;
	}

	// to activate the new baud rate, do reset-and-restart
	// NOTE: The response to this message is at the new baud rate,
	// so it always fails with a timeout
	c4001.resetSystem();

	// continue with the new baud rate
	Serial1.begin(newBaudRate);

	// back to Stop, resetSystem() puts it into Run mode
	return c4001.sensorStop();
}


// Reset the sensor back to its factory default configuration
// NOTE: The baud rate is set back to 9600
bool resetToFactoryDefaults()
{
	if (!c4001.resetCfg())
		return false;

	// activate the new configuration with reset-and-restart
	// NOTE: If the baud rate is changed, the response is returned at 
	// the new baud rate, so it always times out, ignore the response
	c4001.resetSystem();

	// continue with the restored default baud rate, normally 9600
	Serial1.begin(9600);

	// back to stop, resetSystem() puts it into Run mode
	return c4001.sensorStop();
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
	// I use different pins for Serial logging on the STM32 Nucleo-64 boards,
	// else Serial is the USB port which is also used for download
	#ifdef ARDUINO_NUCLEO_64
	Serial.setTx(PC_10);
	Serial.setRx(PC_11);
	#endif

	Serial.begin(115200);
	delay(3000);

	// PuTTY clear screen and scrollback
	// comment out if not using PuTTY
	Serial.print("\033[2J\033[H\033[3J");

	Serial.println("\r\nC4001mmWave-Example Started...\r\n");
	Serial.flush();

	// the sensor's factory setting is 9600 baud
	// modify this after calling changeBaudRate() to set the new rate
	Serial1.begin(9600);

	#if defined(ARDUINO_ARCH_ESP32)
	// for the ESP32s, Serial1's pins can be defined
	Serial1.setPins(6 /*rxPin*/, 7 /*txPin*/);
	#endif

	// the sensor remains in Stop after this, ready to receive commands 
	// failure could be because the baud rate is wrong in Serial1.begin(xxx)
	if (!c4001.begin(&Serial1))
		fatal("begin() failed");

	// display some information about the board
	char hwVersion[32];
	c4001.getHWV(hwVersion);
	char swVersion[32];
	c4001.getSWV(swVersion);
	ulong baudRate;
	c4001.getUart(&baudRate);


	// enable this to permanently change the baud rate
	// Don't forget to update the baud rate in the Serial1.begin() call 
	// above for the next run!
	#if 0
	if (!changeBaudRate(9600))		// the desired baud rate, 4800..115200 
		fatal("changeBaudRate() failed, invalid baud rate?");
	c4001.getUart(&baudRate);
	Serial.print("Baud rate changed to: ");
	Serial.println(baudRate);
	Serial.flush();
	#endif


	// enable this to reset the configuration to factory defaults
	// NOTE! This also restores the baud rate to 9600!
	#if 0
	if (!resetToFactoryDefaults())
		fatal("resetToFactoryDefaults() failed");
	Serial.println("Configuration reset to factory defaults");
	Serial.flush();
	#endif


	// Configure the sensor

	#if 1

	// Presence Detection mode

	// un-comment this if you want to read the current configuration
	/*
	float minMetres, maxMetres;
	c4001.getRange(&minMetres, &maxMetres);
	float minMetres, maxMetres;
	c4001.getRange(&minMetres, &maxMetres);
	float trigRange;
	c4001.getTrigRange(&trigRange);
	uint trigSens, holdSens;
	c4001.getSensitivity(&trigSens, &holdSens);
	float confirmationLatency, disappearanceLatency;
	c4001.getLatency(&confirmationLatency, &disappearanceLatency);
	float inhibitSecs;
	c4001.getInhibit(&inhibitSecs);
	*/
	c4001.setRange(0.6f, 6.0f);		// minMetres is "not recommended to modify"
	c4001.setTrigRange(6.0f);
	c4001.setSensitivity(5, 7);
	c4001.setLatency(0.05f, 15.0f);
	c4001.setInhibit(1.0f);

	if (!c4001.setRunApp(c4001.PresenceDetectionMode))
		fatal("retRunApp() failed");

	#else

	// Motion Detection mode

	// un-comment this if you want to read the current configuration
	/*
	float minMetres, maxMetres;
	c4001.getRange(&minMetres, &maxMetres);
	bool microMotionEnabled;
	c4001.getMicroMotion(&microMotionEnabled);
	uint thresholdFactor;
	c4001.getThresholdFactor(&thresholdFactor);
	*/
	c4001.setRange(0.6f, 6.0f);		// note: minMetres = "not recommended to modify"
	c4001.setMicroMotion(false);
	c4001.setThresholdFactor(5);

	if (!c4001.setRunApp(c4001.MotionDetectionMode))
		fatal("retRunApp() failed");
	#endif


	// enable this to save the configuration to Flash memory
	// this configuration is restored the next time it starts up
	// must be in Stop to do this
	#if 0
	if (!c4001.saveConfig())
		fatal("saveConfig failed");
	// NOTE! sensorStart() must be called to finalize (it's called below)
	#endif

	// start active reporting mode
	// loop() polls for the $DFDxx messages
	c4001.sensorStart();

	Serial.println();
}


// Go!
void loop() 
{
	// active reporting message received?
	if (c4001.receive(NULL)) {

		// (if you already know the mode $DFDMD or $DFHPD, you can skip the strncmp())

		// Motion Detection message
		if (strncmp(c4001.getRxBuf(), "$DFDMD", 6) == 0) {
			uint numTargets;	// always 0 or 1?
			float targetDistance, targetSpeed, targetEnergy;
			c4001.parseDFDMD(&numTargets, &targetDistance, &targetSpeed, &targetEnergy);

			//TODO process motion detection values
		}

		// Presence Detection message
		else if (strncmp(c4001.getRxBuf(), "$DFHPD", 6) == 0) {
			bool presenceDetected;
			c4001.parseDFHPD(&presenceDetected);

			//TODO process presence detection state
		}
	}


	// simple scheduler example (not used)
	#if 0
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
	//if (t2 == 0)	// patch out to do the first call immediately
	//	t2 = ms;
	if (ms - t2 >= 1000) {
		t2 = ms;
		//TODO do something every second
	}
	#endif

}


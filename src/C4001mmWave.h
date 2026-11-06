#pragma once

///////////////////////////////////////////////////////////////////////////////
// Arduino Library for DFRobot C4001 mmWave Presence Sensors
// for DFRobot C4001 mmWave sensors, SKU SEN0609 & SEN0610, UART interface
// 
// Copyright (C) https://muman.ch + https://github.com/mumanchu, 2026.11.06
// If you re-use this software, please include the above copyright notice
//
// For full details see,
// https://github.com/mumanchu/C4001mmWave

#include <string.h>

typedef unsigned long ulong;
typedef unsigned int uint;

// See FloatUtils.cpp
extern bool stringToFloat(const char* psz, float* result, char** pszEnd);
extern bool floatToString(float value, char* psz, unsigned int decimalPlaces);

// For error and data logging
#ifdef DEBUG
#define C4001_LOGERROR(s) { Serial.println(s); Serial.flush(); }
#define C4001_PRINT(s) { Serial.print(s); Serial.flush(); }
#else
#define C4001_LOGERROR(s)
#define C4001_PRINT(s)
#endif


class C4001mmWave
{
protected:
	Stream* stream = NULL;
	char rxBuf[200];	// response message
	char txBuf[100];	// transmit message

	// state machine for receiving messages
	uint rxState = 0;	
	uint rxIndex = 0;
	bool gotResponse = false;

public:

	// Sensor operating modes for setRunApp()
	enum C4001Mode : uint
	{
		PresenceDetectionMode = 0,	// returns $DFHPD messages
		MotionDetectionMode = 1		// returns $DPDMD messages
	};

	bool begin(Stream* serial);

	// Reading Data
	bool parseDFHPD(bool* presenceDetected);
	bool parseDFDMD(uint* numTargets, float* targetDistance, 
		float* targetSpeed, float* targetEnergy);
	bool getHWV(char* hwVersion);
	bool getSWV(char* swVersion);

	// Control
	bool setRunApp(C4001Mode detectionMode);
	bool sensorStop();
	bool sensorStart(bool keepState = false);
	bool saveConfig();
	bool resetCfg();
	bool resetSystem();

	// Configuration
	bool setRange(float minMetres, float maxMetres);
	bool getRange(float* minMetres, float* maxMetres);
	bool setTrigRange(float rangeInMetres);
	bool getTrigRange(float* rangeInMetres);
	bool getSensitivity(uint* holdSensitivity, uint* trigSensitivity);
	bool setSensitivity(uint holdSensitivity, uint trigSensitivity);
	bool setLatency(float confirmationLatency, float disappearanceLatency);
	bool getLatency(float* confirmationLatency, float* disappearanceLatency);
	bool setInhibit(float seconds);
	bool getInhibit(float* seconds);
	bool setUart(ulong baudRate);
	bool getUart(ulong* baudRate);
	bool setMicroMotion(bool enabled);
	bool getMicroMotion(bool* enabled);
	bool setThresholdFactor(uint factor);
	bool getThresholdFactor(uint* factor);

	// These do not work on my boards, but they are in the DFRobot library
	#if 0
	bool setGpioMode(bool mode);		// setIoPolaity()
	bool getGpioMode(bool* mode);		// getIoPolaity()
	bool setPwm(uint pwm1, uint pwm2, uint timer);
	bool getPwm(uint* pwm1, uint* pwm2, uint* timer);
	#endif

	char* getRxBuf() { return rxBuf; }
	bool receive(const char* messageSent);

protected:
	bool parseVersion(bool bsw, char* version);
	bool sendMessageAndGetResponse(const char* message);
	bool getFloats(float* value1, float* value2 = NULL, float* value3 = NULL);
	bool formatFloats(uint numFloats, float value1,	float value2 = 0, float value3 = 0);
	char* getResponseData();
	bool awaitPrompt();
};

// Initialisation
// on return, the sensor is on Stop Mode
// 'Stream* serial' can be the address of a HarwareSerial or SoftwareSerial stream
bool C4001mmWave::begin(Stream* serial)
{
	this->stream = serial;

	// must be powered on for at least 1 second
	while (millis() < 1000)
		yield();

	return sensorStop();
}


///////////////////////////////////////////////////////////////////////////////
// Reading Data

// Parse a $DFHPD message sent in Presence Detection Mode
// Presence detected     "$DFHPD,1, , , *"
// No presence detected  "$DFHPD,0, , , *"
bool C4001mmWave::parseDFHPD(bool* presenceDetected)
{
	if (strncmp(rxBuf, "$DFHPD,", 7) != 0) {
		C4001_LOGERROR("invalid $DFHPD");
		*presenceDetected = false;
		return false;
	}
	*presenceDetected = rxBuf[7] == '1';
	return true;
}

// Parse a $DFDMD message sent in Motion Detection Mode
// (distance and speed detection)
// "$DFDMD,par1,par2,par3,par4,par5,,*"
//   par1 : number of targets  0=no target; 1=one target uint
//   par2 : target number 1 or space
//   par3 : target distance in metres, range 0..25
//   par4 : target speed in metres-per-second, 0..10mps (if par1==1)
//   par5 : target energy (if par1==1)
// Examples:
//   no target   "$DFDMD,0, , , , , , *"
//   has target  "$DFDMD,1,1,0.983,-0.139,16270853, , *"
bool C4001mmWave::parseDFDMD(uint* numTargets, float* targetDistance,
	float* targetSpeed, float* targetEnergy)
{
	*numTargets = 0;
	*targetDistance = 0.0f;
	*targetSpeed = 0.0f;
	*targetEnergy = 0.0f;

	if (strncmp(rxBuf, "$DFDMD,", 7) == 0) {

		// number of targets, currently 0 or 1
		uint targets = rxBuf[7] - '0';
		*numTargets = targets;
		if (targets == 0) {
			// no targets
			return true;
		}

		// three comma-separated floats that describe the target
		while (1) {
			char* psz = rxBuf + 11;
			if (!stringToFloat(psz, targetDistance, &psz))
				break;
			if (*psz++ != ',')
				break;
			if (!stringToFloat(psz, targetSpeed, &psz))
				break;
			if (*psz++ != ',')
				break;
			if (!stringToFloat(psz, targetEnergy, &psz))
				break;
			if (*psz++ != ',')
				break;
			return true;
		}
	}
	C4001_LOGERROR("invalid $DFDMD");
	return false;
}

// Read the hardware version string, max 32 characters including NUL
// e.g. "JYSJ_5807_A01"
bool C4001mmWave::getHWV(char* hwVersion)
{
	*hwVersion = '\0';
	if (!sendMessageAndGetResponse("getHWV"))
		return false;
	return parseVersion(false, hwVersion);
}

// Read the software version string, max. 32 characters including NUL
// e.g. "JYSJ_02.08.08.010827"
bool C4001mmWave::getSWV(char* swVersion)
{
	*swVersion = '\0';
	if (!sendMessageAndGetResponse("getSWV"))
		return false;
	return parseVersion(true, swVersion);
}

// Parse a software or hardware version number 
bool C4001mmWave::parseVersion(bool bsw, char* version)
{
	char* psz = strstr(rxBuf, bsw ? "SoftwareVersion:" : "HardwareVersion:");
	if (psz == NULL)
		return false;
	char* start = psz + 16;
	char* end = strchr(start, '\r');
	if (end == NULL)
		return false;
	*end = '\0';
	uint len = end - start;
	if (len > 31)
		start[31] = '\0';
	strcpy(version, start);
	return true;
}


///////////////////////////////////////////////////////////////////////////////
// Control Commands
// NOTE: The official DFRobot library has delays after each of these,
// but I think you do not need the delays if you wait for the response

// Set the sensor's operating mode and start it.
// It starts running and begins sending $DFHPD or $DFDMD messages,
// so there is no need to use sensorStart().
// The sensor must be stopped to use this command, see sensorStop().
// detectionMode : PresenceDetectionMode for $DFHPD messages
//                 MotionDetectionMode   for $DFDMD messages
bool C4001mmWave::setRunApp(C4001Mode detectionMode)
{
	C4001_PRINT(detectionMode ? "\r\nMotion Detection Mode Set\r\n" : 
		"\r\nPresence Detection Mode Set\r\n");

	sprintf(txBuf, "setRunApp %u\r\n", detectionMode);
	stream->write(txBuf);
	rxState = 0;

	// the protocol document says this command has no response
	// but it responds with "DFRobot:/>", preceded by garbage characters
	// so we wait for the '>', with a timeout
	return awaitPrompt();
}

// Stop the module
// This stops the sending of the "$DFxxx ... *" messages and puts it into 
// configuration mode so the configuration commands can be used
// (?? The response is often "sensor stopped already", even when it was running)
bool C4001mmWave::sensorStop()
{
	return sendMessageAndGetResponse("sensorStop");
}

// Start the module after it was stopped by sensorStop().
// Normally the modified configuration should be saved first with saveConfig()
// keepState : false = start module with no detection (default)
//             true  = start module with last detection state
// (?? The response is often "sensor started already" even when it was stopped)
bool C4001mmWave::sensorStart(bool keepState/*=false*/)
{
	return sendMessageAndGetResponse(keepState ? "sensorStart 1" : "sensorStart");
}

// Save the changed configuration to flash memory
// if not saved, changes will be lost on the next restart
bool C4001mmWave::saveConfig()
{
	return sendMessageAndGetResponse("saveConfig");
}

// Restore all configuration parametres to their factory default values
// the baud rate reverts to 9600
bool C4001mmWave::resetCfg()
{
	return sendMessageAndGetResponse("resetCfg");
}

// Do a software reset and restart
// "resetSystem 1" would put it into bootloader mode, but we don't need that
bool C4001mmWave::resetSystem()
{
	stream->write("resetSystem 0\r\n");
	rxState = 0;

	// the protocol document says this command has no response
	// but it responds with "DFRobot:/>", preceded by garbage characters
	// so we wait for the '>', with a timeout
	return awaitPrompt();
}


///////////////////////////////////////////////////////////////////////////////
// Configuration Commands
// NOTE: The sensor must be in Stop mode to accept configuration commands, 
// see sensorStop().
// Most commands are not accepted unless the associated mode is active, see 
// setRunApp().
// After changing parameters, call saveConfig() to save the changes to flash 
// memory. If not, the old parameters are restored on the next startup.

// Set/get the main/max detection range in metres
// >>> NOTE
// This accesses different data depending on the mode:
//   In Presence Detection Mode it sets the range for presence detection.
//   In Motion Detection Mode it sets the range for motion detection.
//   Presence detection mode : Response 0.600 6.000
//   Motion detection mode   : Response 0.000 26.000
// <<<
// minMetres : Minimum detection distance in metres, 0.6..25, default=0.6
//             the manual says "not recommended to modify"
// maxMetres : Maximum detection distance in metres, minMetres..25, default=6
bool C4001mmWave::setRange(float minMetres, float maxMetres)
{
	strcpy(txBuf, "setRange ");
	if (!formatFloats(2, minMetres, maxMetres))
		return false;
	return sendMessageAndGetResponse(txBuf);
}
bool C4001mmWave::getRange(float* minMetres, float* maxMetres)
{
	*minMetres = 0.6f;	// defaults
	*maxMetres = 6.0f;
	if (!sendMessageAndGetResponse("getRange"))
		return false;
	return getFloats(minMetres, maxMetres);
}

// Set/get the Trigger Distance in metres at which it triggers when a presence 
// is detected
// **Presence Detection Mode only**
// rangeInMetres : 0..25m, default = 6m
bool C4001mmWave::setTrigRange(float rangeInMetres)
{
	strcpy(txBuf, "setTrigRange ");
	floatToString(rangeInMetres, txBuf + 13, 3);
	return sendMessageAndGetResponse(txBuf);
}
bool C4001mmWave::getTrigRange(float* rangeInMetres)
{
	*rangeInMetres = 6.0f;	// default
	if (!sendMessageAndGetResponse("getTrigRange"))
		return false;
	return getFloats(rangeInMetres);
}

// Set Trigger and Hold Sensitivity
// **Presence Detection Mode only**
// Trigger sensitivity = sensitivity of presence detection in sensing area
// Hold sensitivity    = sensitivity of continued presence detection
// higher numbers increase sensitivity, lower numbers decrease sensitivity
// lower sensitivity requires larger movement closer to the sensor
// range 0..9, 255 = unchanged, typically between 2 and 6
bool C4001mmWave::setSensitivity(uint triggerSensitivity, uint holdSensitivity)
{
	sprintf(txBuf, "setSensitivity %u %u", holdSensitivity, triggerSensitivity);
	return sendMessageAndGetResponse(txBuf);
}
bool C4001mmWave::getSensitivity(uint* triggerSensitivity, uint* holdSensitivity)
{
	*triggerSensitivity = 5.0;
	*holdSensitivity = 7.0;
	if (!sendMessageAndGetResponse("getSensitivity"))
		return false;
	char* data = getResponseData();
	if (!data)
		return false;
	return sscanf(data, "%u %u", holdSensitivity, triggerSensitivity) == 2;
}

// Set/get the timing for presence and no presence detection, the time until a 
// presence is detected and the time until no presence is detected. This also affects 
// the OUT pin signal.
// **Presence Detection Mode only**
// confirmationLatency  : the time to detect a presence, 0..100 seconds, default=0.05
// disappearanceLatency : the time to detect no presence, 0.5..1500 seconds, default=15
bool C4001mmWave::setLatency(float confirmationLatency, float disappearanceLatency)
{
	strcpy(txBuf, "setLatency ");
	if (!formatFloats(2, confirmationLatency, disappearanceLatency))
		return false;
	return sendMessageAndGetResponse(txBuf);
}
bool C4001mmWave::getLatency(float* confirmationLatency, float* disappearanceLatency)
{
	*confirmationLatency = 0.05f;
	*disappearanceLatency = 15.0f;
	if (!sendMessageAndGetResponse("getLatency"))
		return false;
	return getFloats(confirmationLatency, disappearanceLatency);
}

// Set/get the "configuration blocking time", the time between presence 
// detected/not detected events, in seconds
// **Presence Detection Mode only**
// This is mostly used to extend the on-time of the OUT pin to prevent 
// too many fluctuations.
// range 0.1 .. 255, default = 1 second
bool C4001mmWave::setInhibit(float seconds)
{
	strcpy(txBuf, "setInhibit ");
	if (!formatFloats(1, seconds))
		return false;
	return sendMessageAndGetResponse(txBuf);
}
bool C4001mmWave::getInhibit(float* seconds)
{
	*seconds = 1.0f;
	if (!sendMessageAndGetResponse("getInhibit"))
		return false;
	return getFloats(seconds);
}

// Set/get the baud rate, 4800..115200
// After setting the UART baud rate, parameters must be saved and the module 
// must be reset for the new configuration to take effect, using saveConfig()
// and resetSystem(). The response to resetSystem() is at the NEW baud rate, 
// so it always returns false - ignore the response then set Serial to the 
// new baud rate.
// See changeBaudRate() in the example sketch
bool C4001mmWave::setUart(ulong baudRate)
{
	switch (baudRate) {
	case 4800:
	case 9600:
	case 19200:
	case 38400:
	case 57600:
	case 115200:
		break;
	default:
		C4001_LOGERROR("invalid baudrate");
		return false;
	}
	sprintf(txBuf, "setUart %lu", baudRate);
	return sendMessageAndGetResponse(txBuf);
}
bool C4001mmWave::getUart(ulong* baudRate)
{
	*baudRate = 9600;
	if (!sendMessageAndGetResponse("getUart"))
		return false;
	char* data = getResponseData();
	if (!data)
		return false;
	char* endPtr;
	*baudRate = strtoul(data, &endPtr, 10);
	return *endPtr == '\0';
}

// Configure the sensor for Micro Motion detection
// **Motion Detection Mode only**
// enabled : false = Disable micro motion detection (default)
//           true  = Enable micro motion detection
bool C4001mmWave::setMicroMotion(bool enabled)
{
	sprintf(txBuf, "setMicroMotion %u", enabled);
	return sendMessageAndGetResponse(txBuf);
}
bool C4001mmWave::getMicroMotion(bool* enabled)
{
	*enabled = false;
	if (!sendMessageAndGetResponse("getMicroMotion"))
		return false;
	char* data = getResponseData();
	if (!data)
		return false;
	*enabled == *data == '1';
	return true;
}

// Set/get the detection threshold
// **Motion Detection Mode only**
// Increase threshold to detect larger objects, 
// decrease to detect smaller objects
// default = 5, the range is not specified in the documentation
bool C4001mmWave::setThresholdFactor(uint factor)
{
	sprintf(txBuf, "setThrFactor %u", factor);
	return sendMessageAndGetResponse(txBuf);
}
bool C4001mmWave::getThresholdFactor(uint* factor)
{
	*factor = 5;
	if (!sendMessageAndGetResponse("getThrFactor"))
		return false;
	char* data = getResponseData();
	if (!data)
		return false;
	return sscanf(data, "%u", factor) == 1;
}

// These do not work on my boards, but they are in the DFRobot library
#if 0
bool C4001mmWave::setGpioMode(bool mode)
{
	sprintf(txBuf, "setGpioMode 1 %u", mode);
	return sendMessageAndGetResponse(txBuf);
}
bool C4001mmWave::getGpioMode(bool* mode)
{
	if (!sendMessageAndGetResponse("getGpioMode 1"))
		return false;
	char* data = getResponseData();
	if (!data)
		return false;
	uint a, b;
	if (sscanf(data, "%u %u", &a, &b) != 2)
		return false;
	*mode = (bool)b;
	return true;
}

bool C4001mmWave::setPwm(uint pwm1, uint pwm2, uint timer)
{
	sprintf(txBuf, "setPwm %u %u %u", pwm1, pwm2, timer);
	return sendMessageAndGetResponse(txBuf);
}
bool C4001mmWave::getPwm(uint* pwm1, uint* pwm2, uint* timer)
{
	if (!sendMessageAndGetResponse("getPwm"))
		return false;
	char* data = getResponseData();
	if (!data)
		return false;
	return sscanf(data, "%u %u %u", pwm1, pwm2, timer) == 3;
}
#endif


// A simple state machine to receive incoming messages, 
// either a response to a sent message (messageSent), 
// or a $DFHPD or $DFDMD message (messageSent == NULL). 
// A state machine is needed so it can receive the next message even 
// when it starts in the middle of a message or a message was corrupted.
// messageSent : this is the message for which we need the response, the 
//   response contains the echoed message and it is used for validation. 
//   Use NULL for receiving $DFHPD and $DFDMD messages.
bool C4001mmWave::receive(const char* messageSent)
{
	while (stream->available()) {
		char ch = stream->read();
		C4001_PRINT(ch);

		// $ always restarts
		if (ch == '$')
			rxState = 0;

		switch (rxState) {

		// waiting for start of message
		case 0:
			gotResponse = false;
			// start of $DFHPD or $DFDMD data message
			if (messageSent == NULL) {
				if (ch == '$') {
					rxBuf[0] = ch;
					rxIndex = 1;
					rxState = 1;
				}
				break;
			}
			// first character of command response message
			if (ch == *messageSent) {
				rxBuf[0] = ch;
				rxIndex = 1;
				rxState = 3;
			}
			break;

		// receiving "$....*"
		case 1:
			rxBuf[rxIndex++] = ch;
			if (ch == '*')
				rxState = 2;
			if (ch == '\r' || ch == '\n')
				rxState = 0;
			break;

		// receiving "\r\n"
		case 2:
			if (ch == '\r' || ch == '\n') {
				rxBuf[rxIndex++] = ch;
				if (ch == '\n') {
					rxBuf[rxIndex] = '\0';
					rxState = 0;
					return true;
				}
				break;
			}
			rxState = 0;
			break;

		// receiving response, read up to "\r\nDFRobot:/>"
		case 3:
			// check first 3 characters, just to be safe
			if (rxIndex < 3 && ch != messageSent[rxIndex]) {
				rxState = 0;
				break;
			}
			rxBuf[rxIndex++] = ch;

			if (ch == '>') {
				// response received
				rxBuf[rxIndex] = '\0';
				rxState = 0;
				gotResponse = true;

				C4001_PRINT("\r\nReceived: \"");
				C4001_PRINT(rxBuf);
				C4001_PRINT("\"\r\n");

				return true;
			}
			break;

		default:
			rxState = 0;
			break;
		}

		// message too long
		if (rxIndex >= sizeof(rxBuf)) {
			C4001_LOGERROR("response too long");
			rxState = 0;
		}
	}
	return false;
}


///////////////////////////////////////////////////////////////////////////////
// Internal Methods

// Get 1, 2 or 3 floating point values from the Response in rxBuf
bool C4001mmWave::getFloats(float* value1, float* value2/*=NULL*/, 
	float* value3/*=NULL*/)
{
	*value1 = 0.0f;
	if (value2)
		*value2 = 0.0f;
	if (value3)
		*value3 = 0.0f;

	char* data = getResponseData();
	if (!data)
		return false;
	char* end;
	if (!stringToFloat(data, value1, &end)) {
		C4001_LOGERROR("invalid float 1");
		return false;
	}
	if (!value2)
		return *end == '\0';
	
	if (!stringToFloat(end, value2, &end)) {
		C4001_LOGERROR("invalid float 2");
		return false;
	}
	if (!value3)
		return *end == '\0';
	
	if (!stringToFloat(end, value2, &end)) {
		C4001_LOGERROR("invalid float 3");
		return false;
	}
	return *end == '\0';
}

// Format 1, 2 or 3 floating point values and append them to txBuf
bool C4001mmWave::formatFloats(uint numFloats, 
	float value1, float value2/*=0*/, float value3/*=0*/)
{
	if (numFloats < 1 || numFloats > 3) {
		C4001_LOGERROR("formatFloats error");
		return false;
	}
	char* psz = strchr(txBuf, '\0');
	floatToString(value1, psz, 3);
	if (numFloats == 1)
		return true;
	psz = strchr(psz, '\0');
	*psz++ = ' ';
	floatToString(value2, psz, 3);
	if (numFloats == 2)
		return true;
	psz = strchr(psz, '\0');
	*psz++ = ' ';
	floatToString(value3, psz, 3);
	return true;
}

// Return a pointer to the start of the response data in rxBuf
char* C4001mmWave::getResponseData()
{
	char* start = strstr(rxBuf, "Response ");
	if (start == NULL) {
		C4001_LOGERROR("missing Response");
		return NULL;
	}
	start += 9;
	char* end = strchr(start, '\r');
	if (end == NULL) {
		C4001_LOGERROR("missing \r");
		return NULL;
	}
	*end = '\0';
	return start;
}

// Send a message and return the response in rxBuf
// the response is parsed by the receive() state machine
// returns false if it times out, has not echoed the command,
// or if the response does not contain the word "Done"
bool C4001mmWave::sendMessageAndGetResponse(const char* message)
{
	C4001_PRINT("\r\nSent: \"");
	C4001_PRINT(message);
	C4001_PRINT("\"\r\n");

	// send message
	stream->write(message);
	stream->write("\r\n");

	// wait for response using the state machine, with timeout
	gotResponse = false;
	rxState = 0;
	ulong tstart = millis();

	while (1) {		
		if (receive(message)) {
			if (gotResponse)
				break;
		}
		ulong elapsed = millis() - tstart;
		if (elapsed > 1000) {
			C4001_LOGERROR("rx timed out");
			return false;
		}
	}

	// valid response?
	// the message is echoed back, with added line "Done" or "Error"
	if (strncmp(rxBuf, message, strlen(message)) != 0 ||
		strstr(rxBuf, "\r\nDone\r\n") == NULL) {
		C4001_LOGERROR(rxBuf);
		return false;
	}
	return true;
}

// For some commands, the protocol document says the command has 
// no response - but it does respond with "DFRobot:/>", preceded 
// by some garbage characters, so we wait for '>', with a timeout
bool C4001mmWave::awaitPrompt()
{
	ulong start = millis();
	while (1) {
		while (stream->available()) {
			char ch = stream->read();
			// end of "DFRobot:/>"
			if (ch == '>') {
				C4001_PRINT("Received: \"DFRobot:/>\"\r\n");
				return true;
			}
		}
		if ((millis() - start) > 1500)
			break;
	}
	C4001_LOGERROR("response timeout");
	return false;
}


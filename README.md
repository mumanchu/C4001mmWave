
## Arduino Library for DFRobot C4001 mmWave Presence Sensors 

This library works for these two DFRobot modules,
* SKU **SEN0609**, C4001 mmWave, 25m, UART only
* SKU **SEN0610**, C4001 mmWave, 12m, UART or I2C (switch selectable), this library only supports UART

<img height="300" alt="image" src="images/c4001-boards.jpg" />


<!-- ========================================================================================== -->

## Contents

- [Introduction](#introduction)
- [Operating Modes](#operating-modes)
- [Flash Memory for Storing the Configuration](#flash-memory)
- [Changing the UART Baud Rate](#baud-rate)
- [Class Reference](#class-reference)
- [Example Sketch](#example-sketch)
- [Advantages of This Library](#advantages)
- [Version numbers of boards used for testing](#board-versions)
- [References](#references)
- [Revision History](#revision-history)


<!-- ========================================================================================== -->

<a name="introduction"></a>
## Introduction

Millimetre wave (mmWave) sensors use _Frequency Modulated Continuous Wave_ (FMCW) modulation. They emit a series of continuous-wave signals with gradually changing frequencies and simultaneously receive the reflected signals. Measurements of distance, velocity and angle are determined by analyzing the reflected signals.

Since they use a kind of radar, they can easily see through a plastic protective cover. They can even see through thin partitions and walls, so take this into account when configuring the sensitivity and range so they don't detect people in adjoining rooms.

They are often called '_Human_ Presence Sensors'. But I suspect they will also detect space aliens, dogs, cats, robots, drones, tarantulas, triffids, etc. (Testing in progress ;-)

Both these DFRobot boards use the 'IcLegend S3KM1110' mmWave sensor. This is a system-on-a-chip (SoC) device which has been programmed by DFRobot to support their proprietary UART protocol.

I like these boards because they are easy to configure and they don't need a PC application to set them up. The DFRobot UART protocol makes it simple.

This library supports UART (RX/TX) communications only, using DFRobot's command-line style UART (ASCII) protocol. I2C (for the SEN0609 only) is not supported, but you don't really need it. Other DFRobot modules use different RX/TX protocols (ASCII or binary), this library will not work with those.

Either `SoftwareSerial` or `HardwareSerial` can be used, via a `Stream` pointer, with baud rates of 4800 to 115200. The default baud rate is 9600, but you can reconfigure that up to 115200 baud (see the example sketch).

There are two on-board LEDs. The green LED is on (or flashing) when it's Running in _active reporting mode_. The white LED follows the state of the OUT pin (presence or motion detected). 

Both boards run on 3.3V or 5V. _Do not connect 5V to the board if your Arduino is 3.3V!_

> [!NOTE]
> The smaller 12m range board (SEN0610) has a switch on the back to select I2C or UART communications. This must be switched to UART mode. The larger 25m range board (SEN0609) has only UART mode.

<!-- ========================================================================================== -->

<a name="operating-modes"></a>
## Operating Modes

**Stop and Run**

When the sensor is in Stop mode, it waits for command messages from the master on its RX line before returning a response. Commands cannot be sent unless it is in Stop. The `sensorStop()` command puts it into Stop mode.

When the sensor is in Run mode, it continuously sends _report messages_ to the master on its TX line. This is called _active data reporting_. Use `setRunApp(C4001Mode detectionMode)` or `sensorStart()` to put the sensor into Run mode.


**Active Reporting Mode: Presence Detection or Motion Detection**

The detection mode is selected with the `setRunApp(C4001Mode detectionMode)` method. 

_Presence detection_ just returns a boolean value meaning 'presence' or 'no presence' (occupied / not occupied) in a "$DFHPD ..." message. Note that the protocol document sometimes uses _existence detection_ instead of _presence detection_, which is not the same thing (you can exist without being present, but you cannot be present if you don't exist ;-)

_Motion detection_ returns much more detailed information in a "$DFDMD ..." message - the number of moving targets detected, the distance, speed and energy. The 'target energy' is not described in the documentation, it seems to be a relative value which could be used to determine the size of the moving object.

> [!TIP]
> For the 25m version, the protocol document states, "a presence detection range of 16 meters, and a motion detection and distance measurement range of 25 meters". I think this means that beyond 16m, the 25m version works only for motion detection. Both presence and motion detection will work up to 16m, and only motion detection works between 16m and 25m. 

> [!NOTE]
> Another note that I found in the protocol document (page 5), "_The C4001 millimeter wave presence sensor is not specifically designed for distance measurement. The distance is currently not calibrated in the current version, so the distance can only be used as a reference and may have some deviation or error._" So take care with the `targetDistance` value. \
> Page 5 also has this statement that I do not understand. "_The C4001 millimeter wave presence sensor module has a 1m transition zone, which means that if the maximum detection distance is set to 3 meters, it is possible to detect targets between 3~25 meters._" Huh?


When in Run, report messages are sent continuously to the master. These should be polled from `loop()` by calling `C4001::receive()`. Received ASCII messages are then parsed with `parseDFHPD()` or `parseDFDMD()` to return individual values. (in the example sketch, you don't need `strncmp()` if you already know the mode.)

You will probably want to add code to check for changes in state, since `presenceDetected` is returned multiple times for the same presence, and the motion detected messages will probably be the same creature(s) moving about in the sensor's field of view.

The `targetSpeed` indicates the direction of movement. A -ve value means the target is moving towards the sensor. A positive value means it is moving away from the sensor.

`targetEnergy` is not described in the ptotocol document. It seems to be a unitless value which is proportional to the size, distance and speed of the target.


```cpp
void loop() 
{
    // active reporting message received?
    if (c4001.receive(NULL)) {

        // (if you already know the mode $DFDMD or $DFHPD, you can skip the strncmp())

        // Motion Detection message
        if (strncmp(c4001.getRxBuf(), "$DFDMD", 6) == 0) {
            uint numTargets;		// always 0 or 1?
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
}

```

<!-- ========================================================================================== -->

<a name="flash-memory"></a>
## Flash Memory for Storing the Configuration

It is not necessary to configure the chip every time it starts up. Once a suitable configuration has been tested, it can be programmed into Flash memory so it is restored automatically when the chip powers up. To store the current configuration, which includes the baud rate, call `saveConfig()`. The example sketch has example code to do this.

After calling `saveConfig()`, you must send either the command to start the module `sensorStart()`, or send the reset/restart command `resetSystem()`, or power it off and on. The newly configured parameters will take effect only after these steps.

Once configured, you can just examine the state of the OUT Pin. You don't need to poll for _active data reporting_ messages to detect a presence or motion, unless you need the motion detection details. 

If the configuration has been saved to Flash, then you don't even need to connect the RX and TX pins.

> [!TIP]
> The OUT pin is _not_ an open drain output. If using more than one sensor you must use a separate input for each one, you cannot connect several OUT pins to one microcontroller input. Or you could use a multiplexer.

> [!WARNING]
> I found that calling `resetSystem()` several times can hang the sensor. It recovers after about 25 seconds. With the 12m board, it seems to hang predictably after _every 20th reset_, which is weird. \
> **_Do not call `resetSystem()` on every power up, only do this when saving the configuration to Flash._**

<!-- ========================================================================================== -->

<a name="baud-rate"></a>
## Changing the UART Baud Rate

The _mumanchu_ example sketch contains a function called `changeBaudRate(ulong newBaudRate)` which will do this for you and save the new baud rate in Flash memory.

After `changeBaudRate()` has been run, you must modify `Serial1.begin(baudRate)` so the new baud rate will be used the next time the program runs.

To change it back to the default 9600, call `changeBaudRate(9600)` or `resetToFactoryDefaults()`.

> [!NOTE]
> The protocol document says the default baud rate is 115200 bps. This was not the case for my two boards, they were both configured for 9600 bps. The factory reset restores the baud rate to 9600.

<!-- ========================================================================================== -->

<a name="class-reference"></a>
## Class Reference

For details of each method, see the comments in the source code, [C4001mmWave.h](src/C4001mmWave.h).

The method names are the same as the UART commands, so you don't get confused.


```cpp
class C4001mmWave
{
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
    // Both modes
    bool setRange(float minMetres, float maxMetres);
    bool getRange(float* minMetres, float* maxMetres);
    // Presence detection mode
    bool setTrigRange(float rangeInMetres);
    bool getTrigRange(float* rangeInMetres);
    bool getSensitivity(uint* holdSensitivity, uint* trigSensitivity);
    bool setSensitivity(uint holdSensitivity, uint trigSensitivity);
    bool setLatency(float confirmationLatency, float disappearanceLatency);
    bool getLatency(float* confirmationLatency, float* disappearanceLatency);
    bool setInhibit(float seconds);
    bool getInhibit(float* seconds);
    // Motion detection mode
    bool setMicroMotion(bool enabled);
    bool getMicroMotion(bool* enabled);
    bool setThresholdFactor(uint factor);
    bool getThresholdFactor(uint* factor);
    // Baud rate
    bool setUart(ulong baudRate);
    bool getUart(ulong* baudRate);

    char* getRxBuf() { return rxBuf; }
    bool receive(const char* messageSent);
};
```

<!-- ========================================================================================== -->

<a name="example-sketch"></a>
## Example Sketch

[C4001mmWave-Example.ino](examples/C4001mmWave-Example/C4001mmWave-Example.ino)

You will need two Serial ports, `Serial1` for connecting the mmWave device, and the default `Serial` for the Serial Monitor so you can see the data. 

The mmWave board works with 3.3V or 5V. If your MCU is 5V, connect 5V to the mmWave board's VIN pin. If the MCU is 3.3V, connect 3.3V to VIN. Connect GND to GND.

Some MCUs, like the Arduino Uno, do not support more than one hardware serial port, so you must use `SoftwareSerial` for `Serial1`, as shown in the example sketch. You will need to include the `SoftwareSerial` platform library.

Connect the MCU's `Serial1` TX pin to the mmWave board's RX pin, and connect the MCU's RX to the mmWave's TX pin. The comments in the example sketch show the most common `Serial1` pin numbers, according to your MCU. If you have problems, use a `SoftwareSerial Serial1(rxPin, txPin)` channel which can usually use any GPIO pins. Note that `SoftwareSerial` may not work reliably over 38400 Baud.

Parts of the example sketch can be modified or enabled to do different things, see the `#if` statements.

* The definition of the `Serial1` port for the mmWave sensor depends on your MCU and architecture. The example sketch contains definitions for some common MCUs. If in doubt, use `SoftwareSerial Serial1(rxPin, txPin)`.
* The code to change the baud rate can be enabled. It's disabled by default.
* The code to reset to factory defaults can be enabled. This also resets the baud rate to 9600.
* The code to configure it for Presence Detection or Motion Detection can be selected. The default is Presence Detection.
* The code to save the new configuration to Flash can be enabled. It's disabled by default.


<!-- ========================================================================================== -->

<a name="advantages"></a>
## Advantages of This Library

* Uses `Stream` for reading and writing data, so the same code works for both `HardwareSerial` and `SoftwareSerial`.

* Has its own optimised floating point conversion routines to/from character arrays, in `FloatUtils.cpp`.

* Does not use the `String` class and string concatenation. The `String` class is used a lot by the DFRobot library. `String` uses `malloc()`, `realloc()` and `free()`, which is not good practice because memory fragmentation can occur in devices with a small amounts of RAM, and it will silently and randomly crash if this happens. 

* Has full communications error detection, with timeouts. Every method returns `true` (success) or `false` (failed). Data is returned via pointer parameters.

* Uses a state machine for processing UART messages received from the mmWave board. This detects corrupted messages, and works if reading starts in the middle of a message.

* Debug output is enabled/disabled with `#define DEBUG`, using the `C4001_LOGERROR(s)` and `C4001_PRINT(s)` macros. Also validates the configuration parameters using `C4001_ASSERT()`.

* Efficient code. This library does not repeat the same code in multiple methods, which cannot be optimised by the compiler. The DFRobot library contains both UART and I2C code, and it requires the Wire library to be installed, even if you don't use I2C. See DFRobot's `uartI2CFlag`. This is strange because they could have put this into the `DFRobot_C4001_I2C` class and done all the shared work in a base class.

* Useful descriptions of each method, to help understand what it does, and why.

* Advantage or Disadvantage? This library supports only the UART protocol. I2C (for the SEN0610 only) is not supported. I'm not sure why you would need to use I2C, because communications speed is not an issue.

* The example sketch shows how to change the baud rate and save it to Flash memory, see `changeBaudRate(ulong newBaudRate)`.

* The example sketch also shows how the reset the configuration back to the factory defaults, see `resetToFactoryDefaults()`.

<!-- ========================================================================================== -->

<a name="board-versions"></a>
## Version numbers of boards used for testing

You may find there are differences in behaviour between different versions of the board's hardware or firmware. These are the versions for my two boards. Please let me know if you find any differences with your board's behaviour which affects this library.

**SEN0610 12m** \
HardwareVersion: JYSJ_426_A01_H \
SoftwareVersion: JYSJ_01.00.08.240220

**SEN0609 25m** \
HardwareVersion: JYSJ_428_A01_H \
SoftwareVersion: JYSJ_01.00.07.240125

The last value of the SoftwareVersion is probably the date, YYMMDD.

<!-- ========================================================================================== -->

<a name="references"></a>
## References

**UART Protocol Document** (in two places, in case one returns a 404)\
https://dfimg.dfrobot.com/wiki/20522/SEN0609_gravity-c4001-24ghz-mmwave-human-presence-detection-sensor_datasheet_V1.pdf
https://dfimg.dfrobot.com/5ea64bf6cf1d8c7738ad2881/wiki/3b88a4ecd7d7f18918a0fa8ba1f970c6.pdf

**IcLegend S3KM1110 Data Sheet** \
_I could not find one!_

**DFRobot Wikis** \
https://wiki.dfrobot.com/sen0609/ \
https://wiki.dfrobot.com/sen0610/

**DFRobot Official Library** \
https://github.com/DFRobot/DFRobot_C4001


<!-- ========================================================================================== -->

<a name="revision-history"></a>
## Revision History

| Date       | Revision | Changes      |
|:---------- |:---------| :----------- |
| 2026.11.09 | 1.0.0    | Improved state machine to receive and parse messages <br/> Tested on Arduino Due (3.3V) |
| 2026.11.07 | 0.0.1    | Tested on STM32 Nucleo-64, Arduino Zero, Arduino Uno (5V) and ESP32-C3-DevKitC |


<br/>

## Joke of the Week

Was Schrödinger's cat able to think outside the box?

<!--
## Joke of the ~~Week~~ Decade

Kier Starmer
-->


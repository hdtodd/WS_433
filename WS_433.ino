/* -*- mode: c++ ; indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*- */

/** @file
   WS_433 remote weather sensor

   Copyright (C) 2025 H. David Todd <hdtodd@gmail.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
*/

/*
  
   MPL3115A2 and Chronodot communications handled by the I2C library of Wayne
   Truchsess to avoid the lost bits and unsynchronized byte frames that were
   common when using the Wire library, apparently caused by the "repeat-start"
   method used by Freescale but not supported well by Wire.  Highly stable with
   I2C library. MPL3115A2 and Chronodot device handlers rewritten to use the I2C
   library.

   MPL3115A code based on code by: A.Weiss, 7/17/2012, changes by
   Nathan Seidle Sept 23rd, 2013 (SparkFun), and Adafruit's TFT Library

   The OneWire library is used for communication with the DS18B20
   sensor.  MPL3115A2 and Chronodot communications handled by the
   I2C library of Wayne Truchsess to avoid the lost bits and
   unsynchronized byte frames that were common when using the
   Wire library, apparently caused by the "repeat-start" method used
   by Freescale but not supported well by Wire.  Highly stable with
   the I2C library.

   The MPL3115A2 and Chronodot device handlers were rewritten to use the
   I2C library.  The MPL3115A2 library is based on the MPLhelp3115A code
   by A.Weiss, 7/17/2012, with changes by Nathan Seidle Sept 23rd,
   2013 (SparkFun).

   For the ST7735-based TFT display, Adafruit's TFT Library is used.

   Code and revisions to this program by HDTodd:

   ----------------------------------------------------------------------
   Setup and Code Management

   Uses the F() macro to move strings to PROGMEN rather than use from SRAM;
   see
        https : // learn.adafruit.com/memories-of-an-arduino/optimizing-sram
        License : This code is public domain but you buy those original
                  authors(Weiss, Seidle) a beer if you use this and meet them
                  someday (Beerware license).

   Edit .h file if you change connection pins!!!

   Hardware Connections to MPL3115A2 
     -VCC = 3.3V
     -SDA = A4    Add 330 ohm resistor in series if using 5v VCC
     -SCL = A5    Add 330 ohm resistor in series if using 5v VCC
     -GND = GND
     -INT pins can be left unconnected for this demo

   Hardware connections to the DS18B20
     -VCC (red)     = 3.3V DOES NOT USE PARASITIC DATA/POWER
     -GND (black)   = GND
     -DATA (yellow) = A5 with 4K7 ohm pullup resistor to VCC

   Author:
     David Todd, hdtodd@gmail.com
     2025.02
*/

/* clang-format off */

/**
    Omni multisensor protocol for ISM-band remote sensing

    This code defines the ISM-band transmission signaling and
    the message data-formatting protocol for the extensible
    wireless sensor 'omni':
    -  Single transmission signaling protocol
    -  Flexible 64-bit data payload field structure
    -  Extensible to a total of 16 possible multi-sensor data formats

    The 'sensor' is actually a programmed microcontroller (e.g.,
    Raspberry Pi Pico 2 or similar) with multiple possible data-sensor
    attachments.  A packet 'format' field indicates the format of the data
    packet being sent.

    NOTE: the rtl_433 decoder, omni.c, reports the packet format, "fmt" in the
    code here, as "channel", in keeping with the standard nomenclature for
    rtl_433 data fields.  "Format" is a better descriptor for the manner in
    which the field is used here, but "channel" is a better, standard label.

    The omni protocol is OOK modulated PWM with fixed period of 600μs
    for data bits, preambled by four long startbit pulses of fixed period equal
    to 1200μs. It is similar to the Lacrosse TX141TH-BV2.

    A single data packet looks as follows:
    1) preamble - 600μs high followed by 600μs low, repeated 4 times:

         ----      ----      ----      ----
        |    |    |    |    |    |    |    |
              ----      ----      ----      ----

    2) a train of 80 data pulses with fixed 600μs period follows immediately:

         ---    --     --     ---    ---    --     ---
        |   |  |  |   |  |   |   |  |   |  |  |   |   |
             --    ---    ---     --     --    ---     -- ....

    A logical 0 is 400μs of high followed by 200μs of low.
    A logical 1 is 200μs of high followed by 400μs of low.

    Thus, in the example pictured above the bits are 0 1 1 0 0 1 0 ...

    The omni microcontroller sends 4 of identical packets of
    4-pulse preamble followed by 80 data bits in a single burst, for a
    total of 336 bits requiring ~212μs.

    The last packet in a burst is followed by a postamble low
    of at least 1250μs.

    These 4-packet bursts repeat every 30 seconds. 

    The message in each packet is 10 bytes / 20 nibbles:

        [fmt] [id] 16*[data] [crc8] [crc8]

    - fmt is a 4-bit message data format identifier
    - id is a 4-bit device identifier
    - data are 16 nibbles = 8 bytes of data payload fields,
          interpreted according to 'fmt'
    - crc8 is 2 nibbles = 1 byte of CRC8 checksum of the first 9 bytes:
          polynomial 0x97, init 0x00

    A format=0 message simply transmits the core temperature and input power
    voltage of the microcontroller and is the format used if no data
    sensor is present.  For format=0 messages, the message
    nibbles are to be read as:

         fi tt t0 00 00 00 00 00 vv cc

         f: format of datagram, 0-15
         i: id of device, 0-15
         t: Pico 2 core temperature: °C *10, 12-bit, 2's complement integer
         0: bytes should be 0
         v: (VCC-3.00)*100, as 8-bit integer, in volts: 3V00..5V55 volts
         c: CRC8 checksum of bytes 1..9, initial remainder 0x00,
            divisor polynomial 0x97, no reflections or inversions

    A format=1 message format is provided as a more complete example.
    It uses the Bosch BME688 environmental sensor as a data source.
    It is an indoor-outdoor temperature/humidity/pressure sensor, and the
    message packet has the following fields:
        indoor temp, outdoor temp, indoor humidity, outdoor humidity,
        barometric pressure, sensor power VCC.
    The data fields are binary values, 2's complement for temperatures.
    For format=1 messages, the message nibbles are to be read as:

         fi 11 12 22 hh gg pp pp vv cc

         f: format of datagram, 0-15
         i: id of device, 0-15
         1: sensor 1 temp reading (e.g, indoor),  °C *10, 12-bit, 2's complement integer
         2: sensor 2 temp reading (e.g, outdoor), °C *10, 12-bit, 2's complement integer
         h: sensor 1 humidity reading (e.g., indoor),  %RH as 8-bit integer
         g: sensor 2 humidity reading (e.g., outdoor), %RH as 8-bit integer
         p: barometric pressure * 10, in hPa, as 16-bit integer, 0..6553.5 hPa
         v: (VCC-3.00)*100, as 8-bit integer, in volts: 3V00..5V55 volts
         c: CRC8 checksum of bytes 1..9, initial remainder 0x00,
                divisor polynomial 0x97, no reflections or inversions

    When asserting/deasserting voltage to the signal pin, timing
    is critical.  The strategy of this program is to have the
    "playback" -- the setting of voltages at specific times to
    convey information -- be as simple as possible to minimize
    computer processing delays in the signal-setting timings.
    So the program generates a "waveform" as a series of commands
    to assert/deassert voltages and to delay the specified times
    to communicate information.  Those commands are entered into
    an array to represent the waveform.

    The playback, then, just retrieves the commands to assert/deassert
    voltages and delay specific length of time and executes them, with
    minimal processing overhead.

    This program was modeled, somewhat, on Joan's pigpiod (Pi GPIO daemon)
    code for waveform generation.  But because the Arduino-like devices
    are single-process/single-core rather than multitasking OSes, the code
    here does not need to provide for contingencies in that multi-tasking
    environment -- it just generates the waveform description which a subsequent
    code module uses to drive the transmitter voltages.
    */
/* clang-format on */

#define DEBUG // SET TO #undef to disable execution trace

#ifdef DEBUG
#define DBG_begin(...)   Serial.begin(__VA_ARGS__);
#define DBG_print(...)   Serial.print(__VA_ARGS__)
#define DBG_write(...)   Serial.write(__VA_ARGS__)
#define DBG_println(...) Serial.println(__VA_ARGS__)
#else
#define DBG_begin(...)
#define DBG_print(...)
#define DBG_write(...)
#define DBG_println(...)
#endif

#define LOOPTIME 5*1000
//#define LOOPTIME 30*1000

#define TX      3      // Use pin 4 to control transmitter
#define LED    13      // LED active on GPIO 13 when transmitting
#define REPEATS 4      // Number of times to repeat packet in one transmission

// #include <stdio.h>
#include <DHT.h>       // DHT22 temp/humidity
#include <DS18.h>      // DS18B20 temp sensors
#include <MPL3115A2.h> // MPL3115 alt/baro/temp
#include <OneWire.h>
#include <I2C.h>       // for I2C communication
//#include <Wire.h>
//#include <SPI.h>
#include "WS_433.h"    // and our own defs of pins etc

/* "SIGNAL_T" are the possible signal types.  Each signal has a
    type (index), name, duration of asserted signal high), and duration of
    deasserted signal (low).  Durations are in microseconds.  Either or
    both duration may be 0, in which case the signal voltage won't be changed.

    Enumerate the possible signal duration types here for use as indices
    Append additional signal timings here and then initialize them
    in the device initiation of the "signals" array
    Maintain the order here in the initialization code in the device class
*/
enum SIGNAL_T {
    NONE     = -1,
    SIG_SYNC = 0,
    SIG_SYNC_GAP,
    SIG_ZERO,
    SIG_ONE,
    SIG_IM_GAP,
    SIG_PULSE
};

// Structure of the table of timing durations used to signal
typedef struct {
    SIGNAL_T sig_type;   // Index the type of signal
    String sig_name;     // Provide a brief descriptive name
    uint16_t up_time;    // duration for pulse with voltage up
    uint16_t delay_time; // delay with voltage down before next signal
} SIGNAL;

// Use crc8 from rtl_433/src/bit_utils.c rather than table-driven
//   in order to minimize variable storage for Arduino Uno
uint8_t crc8(uint8_t const message[], unsigned nBytes, uint8_t polynomial, uint8_t init)
{
    uint8_t remainder = init;
    unsigned byte, bit;

    for (byte = 0; byte < nBytes; ++byte) {
        remainder ^= message[byte];
        for (bit = 0; bit < 8; ++bit) {
            if (remainder & 0x80) {
                remainder = (remainder << 1) ^ polynomial;
            } else {
                remainder = (remainder << 1);
            }
        }
    }
    return remainder;
}

/* ISM_Device is the base class descriptor for creating transmissions
   compatible with various other specific OOK-PWM devices.  It contains
   the list of signals for the transmitter driver, the procedure needed
   to insert signals into the list, and the procedure to play the signals
   through the transmitter.
*/
class ISM_Device {
  public:
    // These are used by the device object procedures
    //   to process the waveform description into a command list
    SIGNAL_T cmdList[350];
    uint16_t listEnd   = 0;
    String Device_Name = "ISM Device";
    SIGNAL *signals;

    ISM_Device() {};

    // Inserts a signal into the commmand list
    void insert(SIGNAL_T signal)
    {
        cmdList[listEnd++] = signal;
        return;
    };

    // Drive the transmitter voltages per the timings of the signal list
    void playback()
    {
        SIGNAL_T sig;
        for (int i = 0; i < listEnd; i++) {
            sig = cmdList[i];
            if (sig == NONE) { // Terminates list but should never be executed
                DBG_print(F(" \tERROR -- invalid signal in 'playback()': "));
                DBG_print((int)cmdList[i]);
                DBG_print((cmdList[i] == NONE) ? " (NONE)" : "");
                return;
            };
            if (signals[sig].up_time > 0) {
                digitalWrite(TX, HIGH);
                delayMicroseconds(signals[sig].up_time);
            };
            if (signals[sig].delay_time > 0) {
                digitalWrite(TX, LOW);
                delayMicroseconds(signals[sig].delay_time);
            };
        }; // end loop
    }; // end playback()
}; // end class ISM_device

class WS_433 : public ISM_Device {

  public:
    // omni timing durations
    // Timings adjusted for Pico 2 based on rtl_433 recording
    // Apparently more delay in pulse & less in gap execution
    /* clang-format off */
    int sigLen             = 6;
    SIGNAL omni_signals[6] = {
            {SIG_SYNC,     "Sync",     600,  600},  // 600, 600
            {SIG_SYNC_GAP, "Sync-gap", 200,  800},  // 200, 800
            {SIG_ZERO,     "Zero",     400,  200},  // 400, 200
            {SIG_ONE,      "One",      200,  400},  // 200, 400
            {SIG_IM_GAP,   "IM_gap",     0, 1250},
            {SIG_PULSE,    "Pulse",      0,    0}   // spare
    };
    /* clang-format on */

    // Instantiate the device by linking 'signals' to our device timing
    WS_433()
    {
        Device_Name = F("WS_433");
        signals     = omni_signals;
    };

    /* clang-format off */
    /* Routines to create 80-bit omni datagrams from sensor data
       Pack <fmt, id, iTemp, oTemp, iHum, oHum, press, volts> into a 72-bit
         datagram appended with a 1-byte CRC8 checksum (10 bytes total).
         Bit fields are binary-encoded, most-significant-bit first.

         Inputs:
         uint8_t  <fmt>   is a 4-bit unsigned integer datagram type identifier
         uint8_t  <id>    is a 4-bit unsigned integer sensor ID
         uint16_t <temp>  is a 16-bit signed twos-complement integer representing
                          10*(temperature reading)
         uint8_t  <hum>   is an 8-bit unsigned integer
                          representing the relative humidity as integer
         uint16_t <press> is a 16-bit unsigned integer representing
                          10*(barometric pressure) (in hPa)
         uint16_t <volts> is a 16-bit unsigned integer
                          representing 100*(voltage-3.00) volts
         uint8_t  <msg>   is an array of at least 10 unsigned 8-bit
                          uint8_t integers

         Output in "msg" as nibbles:

             fi 11 12 22 hh gg pp pp vv cc

             f: format of datagram, 0-15
             i: id of device, 0-15
             1: sensor 1 temp reading (e.g, indoor),  °C *10, 2's complement
             2: sensor 2 temp reading (e.g, outdoor), °C *10, 2's complement
             h: sensor 1 humidity reading (e.g., indoor),  %RH as integer
             g: sensor 2 humidity reading (e.g., outdoor), %RH as integer
             p: barometric pressure * 10, in hPa, 0..65535 hPa*10
             v: (VCC-3.00)*100, in volts,  000...255 volts*100
             c: CRC8 checksum of bytes 1..9, initial remainder 0x00,
                    divisor polynomial 0x97, no reflections or inversions
    */
    /* clang-format on */

    void pack_msg(uint8_t fmt, uint8_t id, int16_t iTemp, int16_t oTemp,
            uint8_t iHum, uint8_t oHum, uint16_t press, uint16_t volts,
            uint8_t msg[])
    {
        msg[0] = (fmt & 0x0f) << 4 | (id & 0x0f);
        msg[1] = (iTemp >> 4) & 0xff;
        msg[2] = ((iTemp << 4) & 0xf0) | ((oTemp >> 8) & 0x0f);
        msg[3] = oTemp & 0xff;
        msg[4] = iHum & 0xff;
        msg[5] = oHum & 0xff;
        msg[6] = (press >> 8) & 0xff;
        msg[7] = press & 0xff;
        msg[8] = volts & 0xff;
        msg[9] = crc8(msg, 9, 0x97, 0x00);
        return;
    };

    // Unpack message into data values it represents
    void unpack_msg(uint8_t msg[], uint8_t &fmt, uint8_t &id, int16_t &iTemp,
            int16_t &oTemp, uint8_t &iHum, uint8_t &oHum, uint16_t &press,
            uint8_t &volts)
    {
        if (msg[9] != crc8(msg, 9, 0x97, 0x00)) {
            DBG_println(
                    F("Attempt unpack of invalid message packet: CRC8 checksum error"));
            fmt   = 0;
            id    = 0;
            iTemp = 0;
            oTemp = 0;
            iHum  = 0;
            oHum  = 0;
            press = 0;
            volts = 0;
        }
        else {
            fmt   = msg[0] >> 4;
            id    = msg[0] & 0x0f;
            iTemp = ((int16_t)((((uint16_t)msg[1]) << 8) | (uint16_t)msg[2])) >> 4;
            oTemp =
                    ((int16_t)((((uint16_t)msg[2]) << 12) | ((uint16_t)msg[3]) << 4)) >>
                    4;
            iHum  = msg[4];
            oHum  = msg[5];
            press = ((uint16_t)(msg[6] << 8)) | msg[7];
            volts = msg[8];
        };
        return;
    };

    void make_wave(uint8_t *msg, uint8_t msgLen)
    {
        listEnd = 0;

        // Repeat message "REPEAT" times in one transmission
        for (uint8_t j = 0; j < REPEATS; j++) {
            // Preamble
            for (uint8_t i = 0; i < 4; i++)
                insert(SIG_SYNC);

            // The data packet
            for (uint8_t i = 0; i < msgLen; i++)
                insert(((uint8_t)((msg[i / 8] >> (7 - (i % 8))) & 0x01)) == 0
                                ? SIG_ZERO
                                : SIG_ONE);
        };

        // Postamble and terminal marker for safety
        insert(SIG_IM_GAP);
        cmdList[listEnd] = NONE;
    }; // end .make_wave()
}; // end class omni


// Global variables

WS_433   om;                  // The omni object as a global
int    count   = 0;           // Count the packets sent
bool   first   = true;
boolean haveDHT22, haveMPL3115, haveDS18;
MPL3115A2 baro;               // Barometer/altimeter/thermometer
DHT myDHT22(DHTPIN, DHTTYPE); // create the temp/RH object
DS18 ds18(oneWirePin);        // Create the DS18 object.
dsInfo dsList[DSMAX + 1];     // max number of DS devices + loop guard
int dsCount;                  // Count the number of DS18B20 probes
uint8_t dsResMode = 1;        // use 10-bit for DS1820B precision
void readSensors(struct recordValues *rec);
void reportOut(struct recordValues *rec);
struct recordValues rec;

// -----------------------------------------------------------------
void setup(void)
{
    DBG_begin(9600);
    delay(1000);
    DBG_println(F("WS_433 starting"));
    I2c.begin();                      // join i2c bus
    I2c.setSpeed(1);                  // go fast
    // Force transmit pin and LED pin low as initial conditions
    pinMode(TX, OUTPUT);
    pinMode(LED, OUTPUT);
    digitalWrite(TX, LOW);
    digitalWrite(LED, LOW);
    baro        = MPL3115A2();  // create barometer
    haveMPL3115 = baro.begin(); // is MPL3115 device there?
    if (haveMPL3115) {          // yes, set parameters
        DBG_println(F("Have mpl"));
        baro.setOversampleRate(sampleRate);
        baro.setAltitude(MY_ALTITUDE); // Set with known altitude
    }
    else {
        DBG_println(F("[%WP] MPL3115A2 is NOT connected!"));
    };

    myDHT22.begin();            // Create temp/humidity sensor
    delay(2000);                // Specs require at least at 2 sec
                                // delay before first reading
    haveDHT22 = myDHT22.read(); // see if it's there/operating
    if (!haveDHT22)
        Serial.println(F("[%WP] DHT22 is NOT connected!"));

    haveDS18 = ds18.begin();
    if (!haveDS18) {
        Serial.println(F("[%WP] DS18 probes NOT connected!"));
    }
    else {
        ds18.reset();
        ds18.reset_search();
        delay(dsResetTime);
        // Scan for address of next device on OneWire
        for (dsCount = 0; (dsCount < DSMAX) && ds18.search(dsList[dsCount].addr);
                dsCount++) {
            // We have a device & space to store its info; verify it and record it or
            // discount it We'll discount if CRC isn't valid or if it's not a know
            // DS18 type

            // First, confirm CRC of address to make sure we're getting valid data
            boolean validCRC =
                    (ds18.crc8(dsList[dsCount].addr, 7) == dsList[dsCount].addr[7]);

            // Confirm that it's a DS18 device and assign type
            dsList[dsCount].type = ds18.idDS(dsList[dsCount].addr[0]);

            // Ignore responses with invalid CRCs or from devices we don't know
            if (!validCRC || (dsList[dsCount].type == DSNull) ||
                    (dsList[dsCount].type == DSUnkwn))
                dsList[dsCount--].alive = false;
            else
                dsList[dsCount].alive = true;
        }; // end for (search for DS18 devices)

        // At this point, we've identified the DS18 devices, identified their types,
        // and  recorded their information
        // Make sure we have at least SOME DS18 devices and not too many

        if (dsCount <= 0) {
            Serial.println(F("[%WP] No valid DS18-class devices found!"));
            haveDS18 = false;
        };
        if (dsCount >= DSMAX) {
            Serial.println(
                    F("[%WP] Number of OneWire devices exceeds internal storage limit"));
            Serial.print(F("             Only "));
            Serial.print(--dsCount);
            Serial.print(F(" DS18 devices will be sampled."));
        };

        // set the precisions for DS18 probe samplings
        for (int dev = 0; dev < dsCount; dev++) {
            ds18.reset();
            ds18.select(dsList[dev].addr);
            ds18.setPrecision(dsList[dev].addr, dsResMode);
        };

        // clean up after DS18 device search and setup
        ds18.reset_search(); // reset search,
        delay(dsResetTime);  // must wait at least 250 msec for reset search
    }; // end else (!haveDS)

    //* And finally, announce ourselves
    DBG_println(F("WS_433 Arduino-Based Weather Sensor startup complete"));
} // end setup()

// -----------------------------------------------------------------
void loop(void)
{
    uint8_t omniLen = 80;  // omni messages are 80 bits long
    uint8_t msg[10] = {0}; // and 10 bytes long
    uint8_t fmt, id = 13, ihum, ohum, volts;
    int16_t itemp, otemp;
    uint16_t press;
    float itempf, otempf, ihumf, ohumf, pressf, voltsf;
    //    reportOut(&rec);

    if (first) {
        DBG_println(F("\nStarting WS_433 transmission"));
    };

    readSensors(&rec);
    DBG_println(F("finished reading sensors"));
    // Pack readings for ISM transmission
    fmt   = 1;
    for (uint8_t dev = 0; dev < dsCount; dev++) {
        if( (rec.ds18.label[dev][0] == 'I')
            && (rec.ds18.label[dev][1] == 'N') )
            itemp = (uint16_t)( ( rec.ds18.tempf[dev] + 0.05 ) * 10 );
        if( (rec.ds18.label[dev][0] == 'O')
            && (rec.ds18.label[dev][1] == 'U') )
            otemp = (uint16_t)( ( rec.ds18.tempf[dev] + 0.05 ) * 10 );
    };
    ihum  = (uint16_t)((rec.dht.rh + 0.5) * 10.0); // round
    ohum  = 99;
    press = (uint16_t)(rec.mpl.press / 10.0);         // hPa * 10
    volts = 245;
    //            (uint8_t)(100.0 * (((float)analogRead(VSYSPin)) / RES * 3.0 * 3.3 - 3.0) +
    //                   0.5);}
    /*        // Use testing values
    fmt   = 1;
    itemp = (int16_t)((21.5 + 0.05) * 10.0);
    otemp = (int16_t)((-15.0 + 0.05) * 10.0);
    ihum  = 23;
    ohum  = 87;
    press = (uint16_t)(101020.0 / 10.0);         // hPa * 10
    voc   = 0;
    volts = 
            (uint8_t)(100.0 * (4.95 - 3.0) +  0.5);
    */    

    // Pack the message, create the waveform, and transmit
    om.pack_msg(fmt, id, itemp, otemp, ihum, ohum, press, volts, msg);
    om.make_wave(msg, omniLen);
    digitalWrite(LED, HIGH);
    om.playback();
    digitalWrite(LED, LOW);
    digitalWrite(TX, LOW);

    // Write back on serial monitor the readings we're transmitting
    // Validates pack/unpack formatting and reconciliation on rtl_433
    om.unpack_msg(msg, fmt, id, itemp, otemp, ihum, ohum, press, volts);
    itempf = ((float)itemp) / 10.0;
    otempf = ((float)otemp) / 10.0;
    ihumf  = ((float)ihum);
    ohumf  = ((float)ohum);
    pressf = ((float)press);
    voltsf = ((float)volts) / 100.0 + 3.00;
    DBG_print(F("Transmit msg "));
    DBG_print(++count);
    DBG_print(F("\tiTemp="));
    DBG_print(itempf);
    DBG_print(F("˚C"));
    DBG_print(F(", oTemp="));
    DBG_print(otempf);
    DBG_print(F("˚C"));
    DBG_print(F(", iHum="));
    DBG_print(ihumf);
    DBG_print(F("%"));
    DBG_print(F(", oHum="));
    DBG_print(ohumf);
    DBG_print(F("%"));
    DBG_print(F(", Press="));
    DBG_print(pressf / 10.0);
    DBG_print(F("hPa"));
    DBG_print(F(", VCC="));
    DBG_print(voltsf);
    DBG_print(F("volts"));
    DBG_print(F("\tin hex: 0x "));
    for (uint8_t j = 0; j < 10; j++) {
        if (msg[j] < 16)
            DBG_print('0');
        DBG_print(msg[j], HEX);
        DBG_print(' ');
    };
    DBG_println();
    DBG_print(F("\tThe msg packet, length="));
    DBG_print((int)omniLen);
    DBG_print(F(", as a string of bits: "));
    for (uint8_t i = 0; i < omniLen; i++)
        DBG_print(((msg[i / 8] >> (7 - (i % 8))) & 0x01));
    DBG_println();

    first = false;
    delay(LOOPTIME);
}; // end loop()

void readSensors(struct recordValues *rec)
{
    uint8_t data[9];

    digitalWrite(LED, HIGH); // visual sign that we're sampling

    // if we have DS18s, start reading temps now, in parallel
    if (haveDS18)
        ds18.readAllTemps();

    if (haveMPL3115) {
        // v5.3: adjust pressure for calibration and altitude
        rec->mpl.press = baro.readPressure() + MY_ELEV_CORR +
                         MY_CALIB_CORR; // Get MPL3115A2 data
        rec->mpl.alt   = baro.readAltitude();
        rec->mpl.tempf = baro.readTempF();
    }
    else {
        rec->mpl.press = 0.0;
        rec->mpl.alt   = 0.0;
        rec->mpl.tempf = 0.0;
    };

    if (haveDHT22) {
        rec->dht.tempf =
                myDHT22.readTemperature(true); // Get DHT22 data with temp in Fahrenheit
        rec->dht.rh = myDHT22.readHumidity();
    }
    else {
        rec->dht.tempf = 0.0;
        rec->dht.rh    = 0.0;
    };

    // Get data for the DS18's we have, zero the rest
    if (haveDS18) {
        ds18.waitForTemps(convDelay[(int)dsResMode]);
        for (int dev = 0; dev < dsCount; dev++) {
            rec->ds18.tempf[dev] =
                    CtoF(ds18.getTemperature(dsList[dev].addr, data, false));
            rec->ds18.label[dev][0] = data[2];
            rec->ds18.label[dev][1] = data[3];
            rec->ds18.label[dev][2] = 0x00;
        };
    };
    for (int dev = dsCount; dev < DSMAX; dev++) {
        rec->ds18.tempf[dev]    = 0.0;
        rec->ds18.label[dev][0] = rec->ds18.label[dev][1] = '*';
        rec->ds18.label[dev][2]                           = 0x00;
    };
    digitalWrite(LED, LOW);
}; // end readSensors

/*
  void reportOut(struct recordValues *rec)
{

    DBG_print("\',");
    DBG_print(rec->mpl.press, 0);
    DBG_print(",");
    DBG_print(rec->mpl.tempf, 1);
    DBG_print(",");
    DBG_print(rec->dht.tempf, 1);
    DBG_print(",");
    DBG_print(rec->dht.rh, 0);
    for (int dev = 0; dev < dsCount; dev++) {
        DBG_print(",\'");
        DBG_print(rec->ds18.label[dev]);
        DBG_print("\',");
        DBG_print(rec->ds18.tempf[dev], 1);
    };
    // If not DSMAX devices, output dummy
    for (int dev = dsCount; dev < DSMAX; dev++) {
        DBG_print(",");
        DBG_print("\'**\',00.0");
    };
    DBG_println(")");

    for (int dev = 0; dev < dsCount; dev++) {
        DBG_println(F("<DS18>"));
        DBG_print(F("<ds18_lbl>"));
        DBG_print(rec->ds18.label[dev]);
        DBG_println(F("</ds18_lbl>"));
        DBG_print(F("<ds18_temp t_scale=\"F\">"));
        DBG_print(rec->ds18.tempf[dev], 1);
        DBG_println(F("</ds18_temp>"));
        DBG_println(F("</DS18>"));
    };
    // If not DSMAX devices, output dummy
    for (int dev = dsCount; dev < DSMAX; dev++) {
        DBG_println(F("<DS18>\n<ds18_lbl>**</ds18_lbl>\n<ds18_temp "
                         "t_scale=\"F\">00.0</ds18_temp>\n</DS18>"));
    };
}; // end void reportOut()
*/

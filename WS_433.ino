/* -*- mode: c++ ; indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-
   WS_433 v1.0
*/

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

   Hardware Connections to MPL3115A2 (Breakoutboard to Arduino):
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
/* clang-format on */

#define LOOPTIME 5 * 1000
// #define LOOPTIME 5*60*1000

#define samplingLED 13

// #include <stdio.h>
#include <DHT.h>       // DHT22 temp/humidity
#include <DS18.h>      // DS18B20 temp sensors
#include <MPL3115A2.h> // MPL3115 alt/baro/temp
#include <OneWire.h>
#include <I2C.h> // for I2C communication
#include <SPI.h>
#include "WS_433.h" // and our own defs of pins etc

MPL3115A2 baro;               // Barometer/altimeter/thermometer
DHT myDHT22(DHTPIN, DHTTYPE); // create the temp/RH object
DS18 ds18(oneWirePin);        // Create the DS18 object.

dsInfo dsList[DSMAX + 1]; // max number of DS devices + loop guard

boolean haveDHT22, haveMPL3115, haveDS18;
int dsCount;
uint8_t dsResMode = 1; // use 10-bit for precision
void readSensors(struct recordValues *rec);
void reportOut(struct recordValues *rec);

/*
 * setup() procedure
 -----------------------------------------------------------------
*/
void setup(void)
{
    Serial.begin(9600, SERIAL_8N1);
    delay(1000);
    Serial.println(F("WS_433 starting"));

    pinMode(samplingLED, OUTPUT);
    digitalWrite(samplingLED, LOW);
    baro        = MPL3115A2();  // create barometer
    haveMPL3115 = baro.begin(); // is MPL3115 device there?
    if (haveMPL3115) {          // yes, set parameters
        baro.setOversampleRate(sampleRate);
        baro.setAltitude(MY_ALTITUDE); // Set with known altitude
    }
    else {
        Serial.println(F("[%WP] MPL3115A2 is NOT connected!"));
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
        // and
        //   recorded their information
        // Make sure we have at least SOME DS18 devices and that we didn't have too
        // many

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
    Serial.println(F("<Starting WS_433 Arduino-Based Weather Sensor"));
} // end setup()

/*
 * loop() procedure
 -----------------------------------------------------------------

*/
void loop(void)
{
    struct recordValues rec;
    readSensors(&rec);
    reportOut(&rec);
    delay(LOOPTIME);
}; // end loop()

void reportOut(struct recordValues *rec)
{

    Serial.print("\',");
    Serial.print(rec->mpl.press, 0);
    Serial.print(",");
    Serial.print(rec->mpl.tempf, 1);
    Serial.print(",");
    Serial.print(rec->dht.tempf, 1);
    Serial.print(",");
    Serial.print(rec->dht.rh, 0);
    for (int dev = 0; dev < dsCount; dev++) {
        Serial.print(",\'");
        Serial.print(rec->ds18.label[dev]);
        Serial.print("\',");
        Serial.print(rec->ds18.tempf[dev], 1);
    };
    // If not DSMAX devices, output dummy
    for (int dev = dsCount; dev < DSMAX; dev++) {
        Serial.print(",");
        Serial.print("\'**\',00.0");
    };
    Serial.println(")");

    for (int dev = 0; dev < dsCount; dev++) {
        Serial.println(F("<DS18>"));
        Serial.print(F("<ds18_lbl>"));
        Serial.print(rec->ds18.label[dev]);
        Serial.println(F("</ds18_lbl>"));
        Serial.print(F("<ds18_temp t_scale=\"F\">"));
        Serial.print(rec->ds18.tempf[dev], 1);
        Serial.println(F("</ds18_temp>"));
        Serial.println(F("</DS18>"));
    };
    // If not DSMAX devices, output dummy
    for (int dev = dsCount; dev < DSMAX; dev++) {
        Serial.println(F("<DS18>\n<ds18_lbl>**</ds18_lbl>\n<ds18_temp "
                         "t_scale=\"F\">00.0</ds18_temp>\n</DS18>"));
    };
}; // end void reportOut()

void readSensors(struct recordValues *rec)
{
    uint8_t data[9];

    digitalWrite(samplingLED, HIGH); // visual sign that we're sampling

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
    digitalWrite(samplingLED, LOW);
}; // end readSensors

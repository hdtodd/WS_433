// Definitions used by the Arduino Uno Weather Sensor,  WS_433.ino
//
#define Vers "WS_433 v1.0"    // <Code-version> 

/* Summary of Uno pin assignments:
   D2   DHT22 data line
   D3   433Mhz XMT data line
   D5   DS18 data line (OneWire data line)
   A4   MPL3115 SDA line (Uno SDA pin)
   A5   MPL3115 SCL line (Uno SCL line)

   Adafruit MPL3115 has level-shifting; no resistor needed
   to use 5V as VCC
*/

// DS18 pin definitions and parameters
// We use powered rather than parasitic mode
//     DS18 data wire to Uno pin 5
//     with 4K7 Ohm pullup to VCC 5V
//     VCC to Uno 5v, GND to Uno GND
#define oneWirePin 5          // Uno pin 5 for OneWire connections to DS18B20
#define dsResetTime 250       // delay time req'd after search reset in msec
#define DSMAX 4               // max number of devices we're prepared to handle

// DHT22 pin definitions and parameters
//     Data pin to Uno pin 2
//     connect a 4K7-10K pull-up resistor between VCC 5v and data pin
//     VCC to Uno 5v, GND to Uno GND
#define DHTPIN 2

#define DHTTYPE DHT22         // the model of our sensor
                              // Alternates: DHT11 or DHT21

// MPL3115A2 pin definitions and parameters
//    SCL to Uno pin A5 (SCL)
//    SDA to Uno pin A4 (SDA)
//    VCC to Uno 5v, GND to Uno GND
uint8_t sampleRate=S_128;     // Sample 128 times for each MPL3115A2 reading
#define MY_ALTITUDE 183       // Set to your GPS-verified altitude in meters
                              // if you want altitude readings to be corrected
                              // Bozeman, MT = 1520m, Williston VT = 169m, Colchester CT = 183
#define MY_ELEV_CORR 18.0*100 // millibars-> Pascals, From Table 2,
                              //   https://www.starpath.com/downloads/calibration_procedure.pdf
#define MY_CALIB_CORR 3.5*100 // millibars-> Pascals, calibrated for my MPL: replace with yours
#define FT_PER_METER 3.28084  // conversion

struct mplReadings {
  float alt;
  float press;
  float tempf;
};

struct dhtReadings {
  float tempf;
  float rh;
};

struct dsReadings {
  char label[DSMAX][3];
  float tempf[DSMAX];
};

struct recordValues {
  struct mplReadings mpl;
  struct dhtReadings dht;
  struct dsReadings ds18;
    };

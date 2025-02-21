// Definitions used by the Arduino Uno Weather Sensor,  WS_433.ino
//
#define Vers "WS_433 v1.0"    // <Code-version> 

// DS18 pin definitions and parameters
                              // We use powered rather than parasitic mode
                              // VCC to Uno 5v, GND to Uno GND
                              // Connect the DS18 data wire to Uno pin 5
                              //  with 4K7 Ohm pullup to VCC 5V
#define dsResetTime 250       // delay time req'd after search reset in msec
#define DSMAX 4               // max number of devices we're prepared to handle
#define oneWirePin 5          // We'll use Uno pin 5 for OneWire connections to DS18B20

// DHT22 pin definitions and parameters
                              // VCC to Uno 5v, GND to Uno GND
#define DHTPIN 2              // Data pin to Uno pin 2
                              // connect a 4K7-10K pull-up resistor between VCC 5v and data pin
#define DHTTYPE DHT22         // the model of our sensor -- could be DHT11 or DHT21

// MPL3115A2 pin definitions and parameters
                              // VCC to Uno 5v, GND to Uno GND
                              // SCL to Uno SCL
                              // SDA to Uno SDA
uint8_t sampleRate=S_128;     // Sample 128 times for each MPL3115A2 reading

#define MY_ALTITUDE 183       // Set this to your actual GPS-verified altitude in meters
                              // if you want altitude readings to be corrected
                              // Bozeman, MT = 1520m, Williston VT = 169m, Colchester CT = 183
#define MY_ELEV_CORR 18.0*100 // millibars-> Pascals, From Table 2,
                              //   https://www.starpath.com/downloads/calibration_procedure.pdf
#define MY_CALIB_CORR 3.5*100 // millibars-> Pascals, calibrated for my MPL: replace with yours
#define FT_PER_METER 3.28084  // conversion

uint32_t readwrite8(uint8_t cmd, uint8_t bits, uint8_t dummy);

struct fieldDesc {
  const char *fieldName; const char *fieldAttributes; };
struct fieldDesc fieldList[] = {
  {"date_time", "TEXT PRIMARY KEY"},
  {"mpl_press", "INT"},
  {"mpl_temp",  "REAL"},
  {"dht22_temp","REAL"},
  {"dht22_rh",  "INT"},
  NULL,        NULL };

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

#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_GPS.h>
#include <HardwareSerial.h>
#include "SdFat.h"
#include "SPI.h"

// ----------- SD Card Pin Definitions -----------

#define SCK   14
#define MISO  12
#define MOSI  13
#define CS    5

// ----------- SD Card Globals -----------

SPIClass spi = SPIClass(HSPI);
SdExFat sd;
const char *filename = "/pitaplatedata.csv";
bool sdInitialized = false;
bool fileReady = false;

// ------------------- Temp Sensor Specifications -------------------

#define TMP1075_ADDR 0x48   // Address is 48 NOT 49 like on excel sheet
#define TMP1075_TEMP_REG 0x00 // Given

// function necessary for reading from sensor and converts it to Celcius
float readTemperature() {
  // Point to temperature register
  Wire.beginTransmission(TMP1075_ADDR);
  Wire.write(TMP1075_TEMP_REG);
  if (Wire.endTransmission(false) != 0) {
    Serial.println("I2C write failed");
    return NAN;
  }

  // Request 2 bytes
  if (Wire.requestFrom(TMP1075_ADDR, 2) != 2) {
    Serial.println("I2C read failed");
    return NAN;
  }

  uint8_t msb = Wire.read();
  uint8_t lsb = Wire.read();

  // TMP1075 gives a 12-bit left-justified signed value
  int16_t raw = (msb << 8) | lsb;
  raw >>= 4;

  // Convert to °C (0.0625°C per LSB)
  return raw * 0.0625;
}

// ------------------- Barometer Specifications (Baro: BMP581) -------------------

#include <Adafruit_BMP5xx.h> //Barometer Library
Adafruit_BMP5xx bmp; //Initialize Baromter object as bmp
#define BMP581_I2C_ADDR 0x46 //Provide Barometer Address

// ------------------- Accelerometer Specifications (High-g Accel: ADXL375BCCZ(?)) -------------------

#define ACCEL_ADDRESS 0x53  // ADXL375 I2C address
#define REG_DATA_FORMAT 0x31 
#define REG_DATAX0 0x32

//These functions just set up Accelerometer

void writeRegister(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(ACCEL_ADDRESS);
  Wire.write(reg);
  Wire.write(value);
  Wire.endTransmission();
}

int16_t readAxis(uint8_t reg) {
  Wire.beginTransmission(ACCEL_ADDRESS);
  Wire.write(reg);
  Wire.endTransmission(false);

  Wire.requestFrom(ACCEL_ADDRESS, (uint8_t)2);
  uint8_t l = Wire.read();
  uint8_t h = Wire.read();

  return (int16_t)((h << 8) | l);
}

// ------------------- IMU Specifications (Imu: ICM-20948) -------------------

#define ICM_ADDR 0x68  // IMU address
#define WHO_AM_I 0x00 // I don't really know what this is..
#define ACCEL_XOUT_H 0x2D //Accel outputs address
#define GYRO_XOUT_H  0x33 //Hyro outputs address
#define PWR_MGMT_1   0x06 // Idk...

int16_t read16(uint8_t reg) {
  Wire.beginTransmission(ICM_ADDR);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom(ICM_ADDR, 2);
  int16_t val = 0;
  if (Wire.available()) {
    val = Wire.read() << 8;
    val |= Wire.read();
  }
  return val;
}
// IMU measurements are stored as 2 consecutive 8 bit bytes
// Takes 2 bytes and combines then into 16 bit measurement


// ------------------- GPS Specifications -------------------

#define GPS_RX_PIN 35  // GPS TX → ESP32 RX
#define GPS_TX_PIN 32  // GPS RX → ESP32 TX
#define GPS_BAUD   9600 //Baud required for GPS
#define TIME_ZONE  -5  // EST offset from UTC

HardwareSerial GPSSerial(1);    // UART1
Adafruit_GPS GPS(&GPSSerial);


// ------------------- Define I2C Pins -------------------

#define SDA_PIN 33
#define SCL_PIN 25

// ------------------- Calibrations -------------------

#define ACCEL_SENS 16384.0   // LSB/g
#define GYRO_SENS 32.8       // LSB/°/s

//Barometer appears calibrated by default!

#define HIGH_G_ACCEL_SENS 20.5 //I'm a little unsure about this value

#define GPS_SPEED_SENS 0.514444 // Converts knots to m/s


// ------------------- Start Data Organisation/Structs -------------------

struct IMUData {
    float ax;
    float ay;
    float az;
    float gx;
    float gy;
    float gz;
};

struct BarometerData {
    float baro_temp;
    float pressure;
};

struct GPSData {
    float latitude;
    float longitude;
    float altitude;
    float speed;
    float cog;
    float num_sats;
    float fix_status;
};

struct HighGAccel{
  float h_ax;
  float h_ay;
  float h_az;
};

struct TempSensor{
  float temp;
};

IMUData imu;
BarometerData baro;
GPSData gps;
HighGAccel hga;
TempSensor tempsensor;

// ------------------- End Data Organisation/Structs -------------------

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10); // Doesn't start until Serial starts --> should this be removed?
  Wire.begin(SDA_PIN, SCL_PIN);

  Wire.setClock(100000);  // 100 kHz COME BACK TO THISSSSSS

  //Initialize Temp Sensor:

  Wire.beginTransmission(TMP1075_ADDR);
  if (Wire.endTransmission() == 0) {
    Serial.println("TMP1075 detected!");
  } else {
    Serial.println("TMP1075 not responding.");
  }
  
  //Initialize Barometer 1: (Insure Barometer can be connected to)

  if (!bmp.begin(BMP581_I2C_ADDR, &Wire)){
    Serial.println("Failed to find 'Barometer1'");
    while (1) delay (10);
  }
  Serial.println("Barometer 1 found and initialized!");

  //Initialize High g Accelerometer 
  //(Come back to this: is +/- 200g what we want?)

  writeRegister(REG_DATA_FORMAT, 0x0F);  // Full resolution, ±200g 
  writeRegister(0x2D, 0x08);  // Set measure bit to start measurements

  //Initialize IMU

  Wire.beginTransmission(ICM_ADDR);
  Wire.write(PWR_MGMT_1);
  Wire.write(0x01); // Set clock source
  Wire.endTransmission();

  Wire.beginTransmission(ICM_ADDR);
  Wire.write(WHO_AM_I);
  Wire.endTransmission(false);
  Wire.requestFrom(ICM_ADDR, 1);
  if (Wire.available()) {
    byte id = Wire.read();
    Serial.print("WHO_AM_I: 0x");
    Serial.println(id, HEX);
  } else {
    Serial.println("Failed to read WHO_AM_I");
  }

  //Initialize GPS
  GPSSerial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  GPS.begin(GPS_BAUD);

  GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCGGA); // RMC + GGA
  GPS.sendCommand(PMTK_SET_NMEA_UPDATE_1HZ);    // 1 Hz updates


  //Initialize SD CARD

  Serial.println("Initializing SD card...");
  spi.begin(SCK, MISO, MOSI, CS);

  //try SD card init with delay for stabilization
  SdSpiConfig spiConfig(CS, SHARED_SPI, SD_SCK_MHZ(8), &spi);
  delay(10000);
  while (!sd.begin(spiConfig)) {
    Serial.println("Card Mount Failed. Retrying...");
    delay(1000);
  }

  Serial.println("SD Card Mounted Successfully!");
}

void loop() {
  unsigned long timestamp = millis() / 1000;

  unsigned long seconds = timestamp % 60;
  unsigned long minutes = (timestamp / 60) % 60;
  unsigned long hours   = (timestamp / 3600) % 24; 

  //Set GPS Variables

  while (GPS.available()) {
  char incomingChar = GPS.read();
  // Serial.write(incomingChar);  // uncomment to see raw GPS output
  }

  //Performs "safety" check on GPS Data to make sure Data is not curropt/inaccurate
  if (GPS.newNMEAreceived()) {
    if (!GPS.parse(GPS.lastNMEA())) {
      return; // parse failed, skip
    }
  }

  gps.latitude = GPS.latitudeDegrees;
  gps.longitude = GPS.longitudeDegrees;
  gps.altitude = GPS.altitude;
  gps.speed = GPS.speed * GPS_SPEED_SENS; // come back and make this a varr
  gps.cog = GPS.angle;
  gps.num_sats = GPS.satellites;
  gps.fix_status = GPS.fix;

  //keep trying to create or open the file until successful
  while (!fileReady) {
    if (!sd.exists(filename)) {
      Serial.println("File does not exist. Trying to create...");
      ExFile file = sd.open(filename, O_WRONLY | O_CREAT);
      if (file) {
        file.printf("%d/%d/%d %02d:%02d:%02d (UTC %+d)\n",GPS.day, GPS.month, GPS.year,GPS.hour, GPS.minute, GPS.seconds,TIME_ZONE);
        file.println("Timestamp, Barometer Temperature, Barometer Pressure, Temperature, High G Accelerometer X, High G Accelerometer Y, High G Accelerometer Z, IMU Accelerometer X, IMU Accelerometer Y, IMU Accelerometer Z, IMU Gyroscope X, IMU Gyroscope Y, IMU Gyroscope Z, GPS Latitude, GPS Longitude, GPS altitude");
        file.close();
        Serial.println("File created and header written.");
        fileReady = true;
      } else {
        Serial.println("Failed to create file. Retrying...");
        delay(500);
        continue;
      }
    } else {
      //try opening to make sure it can be appended to
      ExFile testFile = sd.open(filename, O_WRONLY | O_APPEND);
      if (testFile) {
        testFile.close();
        Serial.println("File exists and is ready.");
        fileReady = true;
      } else {
        Serial.println("File exists but can't be opened. Retrying...");
        delay(500);
      }
    }
  }

  //Set Baro Variables

  baro.baro_temp = bmp.readTemperature(); // (°C)
  baro.pressure = bmp.readPressure(); // (hPa)

  //Set Temp Sensor Variables

  tempsensor.temp = readTemperature();

  //Set High g Accel Variables

  hga.h_ax = readAxis(REG_DATAX0) / HIGH_G_ACCEL_SENS;
  hga.h_ay = readAxis(REG_DATAX0 + 2) / HIGH_G_ACCEL_SENS;
  hga.h_az = readAxis(REG_DATAX0 + 4) / HIGH_G_ACCEL_SENS;

  //Set IMU Varriables (Calibrated to be in LSB/g and LSB/°/s, respectively )

  imu.ax = read16(ACCEL_XOUT_H) / ACCEL_SENS;
  imu.ay = read16(ACCEL_XOUT_H + 2) / ACCEL_SENS;
  imu.az = read16(ACCEL_XOUT_H + 4) / ACCEL_SENS;

  imu.gx = read16(GYRO_XOUT_H) / GYRO_SENS;
  imu.gy = read16(GYRO_XOUT_H + 2) / GYRO_SENS;
  imu.gz = read16(GYRO_XOUT_H + 4) / GYRO_SENS;


  // Tests by printing to Serial

  // Serial.printf("%d/%d/%d %02d:%02d:%02d (UTC %+d)\n",
  //   GPS.day, GPS.month, GPS.year,
  //   GPS.hour, GPS.minute, GPS.seconds,
  //   TIME_ZONE);

  Serial.printf("Lat, Long, Alt): %.5f, %.5f, %.5f\n", gps.latitude, gps.longitude, gps.altitude);
  // Serial.printf("Accel g's): %.2f, %.2f, %.2f\n", imu.ax, imu.ay, imu.az);
  // Serial.printf("Gyro (°/s): %.2f, %.2f, %.2f\n", imu.gx, imu.gy, imu.gz);
  // Serial.printf("Temp Sensor Temp: %.2f\n", tempsensor.temp);
  // Serial.printf("Baro Temp: %.2f\n", baro.baro_temp);
  // Serial.printf("Baro Pressure: %.2f\n", baro.pressure);
  // Serial.printf("High G Accel: %.2f, %.2f, %.2f\n", hga.h_ax, hga.h_ay, hga.h_az);


  ExFile file = sd.open(filename, O_WRONLY | O_APPEND);
  if (!file) {
    Serial.println("Failed to append to file. Will retry file setup...");
    fileReady = false;  
    delay(500);
    return;
  }

  //Need to go through and determine how many decimals we need for each

  file.print("%02lu:%02lu:%02lu,", hours, minutes, seconds); //temporary solution to maintaining a shared clock
  file.print(",");
  file.print(baro.baro_temp);
  file.print(",");
  file.print(baro.pressure);
  file.print(",");
  file.print(tempsensor.temp);
  file.print(",");
  file.print(hga.h_ax);
  file.print(",");
  file.print(hga.h_ay);
  file.print(",");
  file.print(hga.h_az);
  file.print(",");
  file.print(imu.ax);
  file.print(",");
  file.print(imu.ay);
  file.print(",");
  file.print(imu.az);
  file.print(",");
  file.print(imu.gx);
  file.print(",");
  file.print(imu.gy);
  file.print(",");
  file.print(imu.gz);
  file.print(",");
  file.print(gps.latitude, 5);
  file.print(",");
  file.print(gps.longitude, 5);
  file.print(",");
  file.print(gps.altitude, 5);
  file.println();

  file.close();
  Serial.println("Data logged.");
  
  delay(500);
}



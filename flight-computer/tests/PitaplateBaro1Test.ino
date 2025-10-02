#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP5xx.h>

// Create BMP5xx object
Adafruit_BMP5xx bmp;

// I2C pins
#define I2C_SDA 33
#define I2C_SCL 25

// BMP581 I2C address
#define BMP581_I2C_ADDR 0x46

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);
  Serial.println("ESP32 + BMP581 test");

  // Initialize I2C on custom pins
  Wire.begin(I2C_SDA, I2C_SCL);

  // Try to initialize BMP581
  if (!bmp.begin(BMP581_I2C_ADDR, &Wire)) {
    Serial.println("Failed to find BMP581 chip!");
    while (1) delay(10);
  }
  Serial.println("BMP581 found and initialized!");
}

void loop() {
  // Read pressure (in Pa) and temperature (in °C)
  float temperature = bmp.readTemperature();
  float pressure    = bmp.readPressure();

  if (!isnan(temperature) && !isnan(pressure)) {
    Serial.print("Temperature = ");
    Serial.print(temperature, 3);
    Serial.println(" °C");

    Serial.print("Pressure = ");
    Serial.print(pressure, 3);
    Serial.println(" hPa");
  } else {
    Serial.println("Failed to read data from BMP581!");
  }

  delay(1000); // 1s update
}

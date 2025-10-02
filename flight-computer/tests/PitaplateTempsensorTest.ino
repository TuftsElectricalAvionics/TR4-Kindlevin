#include <Wire.h>

#define TMP1075_ADDR 0x48   // confirmed by I2C scanner
#define TMP1075_TEMP_REG 0x00

#define I2C_SDA 33
#define I2C_SCL 25

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("Initializing TMP1075...");

  Wire.begin(I2C_SDA, I2C_SCL, 100000); // safer to start at 100kHz

  // Test connection
  Wire.beginTransmission(TMP1075_ADDR);
  if (Wire.endTransmission() == 0) {
    Serial.println("TMP1075 detected!");
  } else {
    Serial.println("TMP1075 not responding.");
  }
}

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

void loop() {
  float tempC = readTemperature();
  if (!isnan(tempC)) {
    Serial.print("Temperature: ");
    Serial.print(tempC, 2);
    Serial.println(" °C");
  }
  delay(1000);
}

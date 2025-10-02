#include <Wire.h>

#define ACCEL_ADDRESS 0x53  // ADXL375 I2C address
#define REG_DATA_FORMAT 0x31
#define REG_DATAX0 0x32

void setup() {
  Serial.begin(115200);
  Wire.begin(33, 25);  // SDA = 33, SCL = 25

  // Set the ADXL375 to ±200g range
  writeRegister(REG_DATA_FORMAT, 0x0F);  // Full resolution, ±200g

  // Wake up the ADXL375
  writeRegister(0x2D, 0x08);  // Set measure bit to start measurements
}

void loop() {
  int16_t x = readAxis(REG_DATAX0);
  int16_t y = readAxis(REG_DATAX0 + 2);
  int16_t z = readAxis(REG_DATAX0 + 4);

  // Convert raw data to g's
  float x_g = x / 20.5;
  float y_g = y / 20.5;
  float z_g = z / 20.5;

  Serial.print("X: "); Serial.print(x_g); Serial.print(" g\t");
  Serial.print("Y: "); Serial.print(y_g); Serial.print(" g\t");
  Serial.print("Z: "); Serial.print(z_g); Serial.println(" g");

  delay(200);
}

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

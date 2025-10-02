#include <Wire.h>

#define ICM_ADDR 0x68  // ICM-20948 address
#define WHO_AM_I 0x00
#define ACCEL_XOUT_H 0x2D
#define GYRO_XOUT_H  0x33
#define PWR_MGMT_1   0x06

// SDA = 33, SCL = 25
#define SDA_PIN 33
#define SCL_PIN 25

// Sensitivities
#define ACCEL_SENS 16384.0   // LSB/g
#define GYRO_SENS 32.8       // LSB/°/s

void setup() {
  Serial.begin(115200);
  Wire.begin(SDA_PIN, SCL_PIN);
  delay(100);

  // Wake up the IMU
  Wire.beginTransmission(ICM_ADDR);
  Wire.write(PWR_MGMT_1);
  Wire.write(0x01); // Set clock source
  Wire.endTransmission();

  // Check WHO_AM_I
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
}

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

void loop() {
  int16_t ax_raw = read16(ACCEL_XOUT_H);
  int16_t ay_raw = read16(ACCEL_XOUT_H + 2);
  int16_t az_raw = read16(ACCEL_XOUT_H + 4);

  int16_t gx_raw = read16(GYRO_XOUT_H);
  int16_t gy_raw = read16(GYRO_XOUT_H + 2);
  int16_t gz_raw = read16(GYRO_XOUT_H + 4);

  // Scale to physical units
  float ax = ax_raw / ACCEL_SENS;
  float ay = ay_raw / ACCEL_SENS;
  float az = az_raw / ACCEL_SENS;

  float gx = gx_raw / GYRO_SENS;
  float gy = gy_raw / GYRO_SENS;
  float gz = gz_raw / GYRO_SENS;

  Serial.print("Accel (g): ");
  Serial.print(ax); Serial.print(", ");
  Serial.print(ay); Serial.print(", ");
  Serial.println(az);

  Serial.print("Gyro (°/s): ");
  Serial.print(gx); Serial.print(", ");
  Serial.print(gy); Serial.print(", ");
  Serial.println(gz);

  delay(500);
}

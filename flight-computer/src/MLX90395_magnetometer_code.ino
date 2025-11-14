// requries the Adafruit MLX90395 library by Adafruit
// .h file for this library can be found at https://github.com/adafruit/Adafruit_MLX90395/blob/master/Adafruit_MLX90395.h


#include "Adafruit_MLX90395.h"

Adafruit_MLX90395 sensor = Adafruit_MLX90395();

void setup(void)
{
  Serial.begin(115200);

  /* Wait for serial on USB platforms. */
  while (!Serial) {
      delay(10);
  }

  Serial.println("MG > Initializing Magnetometer (MG)");
  
  if (! sensor.begin_I2C(0x0C)) {          // hardware I2C mode, can pass in address & alt Wire
    Serial.println("MG > No sensor found ... check your wiring?");
    while (1) { delay(10); }
  }
  Serial.print("MG > Found a MLX90395 sensor with unique id 0x");
  Serial.print(sensor.uniqueID[0], HEX);
  Serial.print(sensor.uniqueID[1], HEX);
  Serial.println(sensor.uniqueID[2], HEX);

  sensor.setOSR(MLX90395_OSR_8);
  Serial.print("MG > OSR set to: ");
  switch (sensor.getOSR()) {
    case MLX90395_OSR_1: Serial.println("1 x"); break;
    case MLX90395_OSR_2: Serial.println("2 x"); break;
    case MLX90395_OSR_4: Serial.println("4 x"); break;
    case MLX90395_OSR_8: Serial.println("8 x"); break;
  }
  
  sensor.setResolution(MLX90395_RES_17);
  Serial.print("MG > Resolution: ");
  switch (sensor.getResolution()) {
    case MLX90395_RES_16: Serial.println("16b"); break;
    case MLX90395_RES_17: Serial.println("17b"); break;
    case MLX90395_RES_18: Serial.println("18b"); break;
    case MLX90395_RES_19: Serial.println("19b"); break;
  }
  
  Serial.print("MG > Gain: "); Serial.println(sensor.getGain());
}

void loop(void) {

  /* Get a new sensor event, normalized to uTesla */
  sensors_event_t event; 
  sensor.getEvent(&event);
  /* Display the results (magnetic field is measured in uTesla) */
  Serial.print("MG > X: "); Serial.print(event.magnetic.x);
  Serial.print(" \tY: "); Serial.print(event.magnetic.y); 
  Serial.print(" \tZ: "); Serial.print(event.magnetic.z); 
  Serial.println(" uTesla ");

  delay(500);
}

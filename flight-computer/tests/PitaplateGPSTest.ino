#include <Adafruit_GPS.h>
#include <HardwareSerial.h>

// --------------------
// Hardware configuration
// --------------------
#define GPS_RX_PIN 35  // GPS TX → ESP32 RX
#define GPS_TX_PIN 32  // GPS RX → ESP32 TX
#define GPS_BAUD   9600
#define TIME_ZONE  -5  // EST offset from UTC

HardwareSerial GPSSerial(1);    // UART1
Adafruit_GPS GPS(&GPSSerial);

// --------------------
// Setup
// --------------------
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("GPS demo started. Waiting for GPS data...");

  GPSSerial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  GPS.begin(GPS_BAUD);

  // Configure GPS output (similar to nmea_parser config in ESP-IDF)
  GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCGGA); // RMC + GGA
  GPS.sendCommand(PMTK_SET_NMEA_UPDATE_1HZ);    // 1 Hz updates
}

// --------------------
// Loop
// --------------------
void loop() {
  // Continuously read characters from GPS
  char c = GPS.read();

  // If a new complete NMEA sentence is available, parse it
  if (GPS.newNMEAreceived()) {
    if (!GPS.parse(GPS.lastNMEA())) {
      return; // parse failed, skip
    }
  }

  // Once per second, print parsed data
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 1000) {
    lastPrint = millis();

    // Print time/date
    Serial.printf("%d/%d/%d %02d:%02d:%02d (UTC %+d)\n",
                  GPS.day, GPS.month, GPS.year,
                  GPS.hour, GPS.minute, GPS.seconds,
                  TIME_ZONE);

    // Print parsed values
    Serial.printf("  latitude   = %.5f°\n", GPS.latitudeDegrees);
    Serial.printf("  longitude  = %.5f°\n", GPS.longitudeDegrees);
    Serial.printf("  altitude   = %.2f m\n", GPS.altitude);
    Serial.printf("  speed      = %.2f m/s\n", GPS.speed * 0.514444); // knots → m/s
    Serial.printf("  cog        = %.2f°\n", GPS.angle);
    Serial.printf("  num sats   = %d\n", GPS.satellites);
    Serial.printf("  fix status = %d\n", GPS.fix);
    Serial.printf("-----------------------------\n");
  }
}

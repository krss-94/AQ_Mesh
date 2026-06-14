/*
  mac_scanner.ino
  Flash this on any ESP32 to get its MAC address in the exact format
  needed for config.h files.

  Board: ESP32 Dev Module
  No external libraries needed.
*/

#include <WiFi.h>

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Must be in STA mode — MAC is stable here
  WiFi.mode(WIFI_STA);
  delay(100);

  uint8_t mac[6];
  WiFi.macAddress(mac);

  Serial.println("\n==============================");
  Serial.println("       ESP32 MAC SCANNER      ");
  Serial.println("==============================");
  Serial.printf("Human-readable:  %s\n", WiFi.macAddress().c_str());
  Serial.println();
  Serial.println("Paste into config.h:");
  Serial.printf("{ 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X }\n",
    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  Serial.println("==============================\n");
}

void loop() {
  // Nothing — open Serial Monitor at 115200 and read once
}

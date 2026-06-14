#include "wifi_uplink.h"
#include "config.h"
#include <WiFi.h>
#include <HTTPClient.h>

void wifi_post(const char* url, const String& json) {
  Serial.println("[UPLINK] Connecting to Wi-Fi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long t = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - t > WIFI_TIMEOUT_MS) {
      Serial.println("[UPLINK] Wi-Fi timeout — skipping POST");
      WiFi.disconnect(true);
      return;
    }
    delay(200);
    Serial.print(".");
  }
  Serial.printf("\n[UPLINK] Connected: %s\n", WiFi.localIP().toString().c_str());

  HTTPClient http;
  http.begin(url);
  http.addHeader("Content-Type", "application/json");

  int code = http.POST(json);
  if (code > 0) {
    Serial.printf("[UPLINK] POST → %d\n", code);
  } else {
    Serial.printf("[UPLINK] POST failed: %s\n", http.errorToString(code).c_str());
  }

  http.end();
  WiFi.disconnect(true);
  Serial.println("[UPLINK] Wi-Fi disconnected");
}

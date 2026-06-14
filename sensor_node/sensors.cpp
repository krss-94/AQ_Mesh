#include "sensors.h"
#include "config.h"
#include <DHT.h>

static DHT dht(DHT_PIN, DHT_TYPE);

void sensors_init() {
  dht.begin();
  pinMode(MQ135_PIN, INPUT);
  Serial.println("[SENSOR] DHT22 + MQ-135 initialized");
  Serial.println("[SENSOR] MQ-135 warming up — wait 30s before trusting CO2 values");
}

void sensors_read(SensorPayload* out) {
  float temp = dht.readTemperature();
  float hum  = dht.readHumidity();

  if (isnan(temp) || isnan(hum)) {
    Serial.println("[SENSOR] DHT22 read failed — check wiring & pull-up resistor");
    temp = -99.0f;
    hum  = 0.0f;
  }

  int raw_adc = analogRead(MQ135_PIN);

  // Linear mapping: MQ135_CLEAN_AIR (clean) → MQ135_PPM_MIN, 4095 → MQ135_PPM_MAX
  int co2 = map(raw_adc, MQ135_CLEAN_AIR, 4095, MQ135_PPM_MIN, MQ135_PPM_MAX);
  co2 = constrain(co2, MQ135_PPM_MIN, MQ135_PPM_MAX);

  out->temperature  = (int16_t)(temp * 100);
  out->humidity     = (uint16_t)(hum * 100);
  out->pm25         = 0;          // no PM sensor yet — zero it out
  out->co2_ppm      = (uint16_t)co2;
  out->voc_raw      = (uint16_t)raw_adc;
  out->air_quality  = compute_aqi(co2, 0);
  out->timestamp_ms = millis();
  memset(out->reserved, 0, sizeof(out->reserved));

  Serial.printf("[SENSOR] Temp:%.2f°C  Hum:%.2f%%  CO2:%dppm  AQI:%d\n",
    temp, hum, co2, out->air_quality);
}

uint8_t compute_aqi(uint16_t co2_ppm, uint16_t pm25_x10) {
  // Simple CO2-based AQI tier
  if      (co2_ppm < 600)  return 0;  // Good
  else if (co2_ppm < 1000) return 1;  // Moderate
  else if (co2_ppm < 1500) return 2;  // Unhealthy for Sensitive Groups
  else if (co2_ppm < 2000) return 3;  // Unhealthy
  else if (co2_ppm < 3000) return 4;  // Very Unhealthy
  else                     return 5;  // Hazardous
}

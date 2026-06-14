/*
  sensor_node.ino
  Secure Air Quality Mesh Cluster — Sensor Node Firmware

  Reads DHT22 + MQ-135 every SEND_INTERVAL_MS, packs into AQPacket,
  sends via ESP-NOW to coordinator (direct or via relay).
  Retries up to MAX_RETRIES times on no ACK.

  Hardware:
    ESP32 DevKit v1 (30-pin)
    DHT22  → GPIO 4  (10K pull-up to 3.3V)
    MQ-135 → GPIO 34 (ADC1, 5V supply on Vin/VBUS)
    OLED   → GPIO 21 (SDA), 22 (SCL)  [optional]
*/

#include <esp_now.h>
#include <WiFi.h>
#include "config.h"
#include "../shared/packet_types.h"
#include "../shared/crc8.h"
#include "sensors.h"

#if USE_OLED
  #include <Adafruit_SSD1306.h>
  Adafruit_SSD1306 oled(128, 64, &Wire, -1);
#endif

// ─── State ────────────────────────────────────────────────────────────────────
static uint8_t  seqNum       = 0;
static bool     ackReceived  = false;
static uint8_t  retryCount   = 0;
static unsigned long lastSend = 0;

// ─── Helpers ──────────────────────────────────────────────────────────────────
void send_packet(AQPacket* pkt) {
  uint8_t* target = USE_RELAY ? RELAY_MAC : COORDINATOR_MAC;
  esp_err_t result = esp_now_send(target, (uint8_t*)pkt, sizeof(AQPacket));
  if (result != ESP_OK) {
    Serial.printf("[TX ERR] esp_now_send failed: %s\n", esp_err_to_name(result));
  }
}

void build_packet(AQPacket* pkt) {
  SensorPayload sp;
  sensors_read(&sp);

  pkt->version     = PKT_VERSION;
  pkt->type        = PKT_DATA;
  pkt->source_id   = NODE_ID;
  pkt->dest_id     = 0xFF;            // coordinator
  pkt->seq_num     = seqNum++;
  pkt->hop_count   = 0;
  pkt->flags       = 0x01;            // ACK_REQ
  pkt->payload_len = sizeof(SensorPayload);
  memcpy(pkt->payload, &sp, sizeof(SensorPayload));
  pkt->crc8        = crc8_compute((uint8_t*)pkt, sizeof(AQPacket) - 1);
}

// ─── Callbacks ────────────────────────────────────────────────────────────────
void onDataSent(const uint8_t* mac, esp_now_send_status_t status) {
  if (status == ESP_NOW_SEND_SUCCESS) {
    Serial.printf("[TX] SEQ:%03d → SENT OK\n", seqNum - 1);
  } else {
    Serial.printf("[TX] SEQ:%03d → FAILED (no peer ACK from ESP-NOW layer)\n", seqNum - 1);
  }
}

void onDataRecv(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
  if (len != sizeof(AQPacket)) return;
  AQPacket* pkt = (AQPacket*)data;

  if (pkt->type == PKT_ACK && pkt->dest_id == NODE_ID) {
    Serial.printf("[ACK] Received from coordinator for SEQ:%03d\n", pkt->seq_num);
    ackReceived = true;
  }
}

// ─── Setup ────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.printf("\n[BOOT] Sensor Node 0x%02X starting...\n", NODE_ID);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  if (esp_now_init() != ESP_OK) {
    Serial.println("[ERR] ESP-NOW init failed — restarting");
    delay(1000);
    ESP.restart();
  }

  esp_now_register_send_cb(onDataSent);
  esp_now_register_recv_cb(onDataRecv);

  // Register coordinator as peer
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, COORDINATOR_MAC, 6);
  peer.channel = ESPNOW_CHANNEL;
  peer.encrypt = false;
  if (esp_now_add_peer(&peer) != ESP_OK) {
    Serial.println("[ERR] Failed to add coordinator peer");
  }

  // Register relay as peer if enabled
  if (USE_RELAY) {
    esp_now_peer_info_t relay = {};
    memcpy(relay.peer_addr, RELAY_MAC, 6);
    relay.channel = ESPNOW_CHANNEL;
    relay.encrypt = false;
    esp_now_add_peer(&relay);
    Serial.println("[PEER] Relay node registered");
  }

  sensors_init();

#if USE_OLED
  Wire.begin(OLED_SDA, OLED_SCL);
  oled.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  oled.clearDisplay();
  oled.setTextColor(WHITE);
  oled.setTextSize(1);
  oled.setCursor(0, 0);
  oled.printf("Node 0x%02X Ready", NODE_ID);
  oled.display();
#endif

  Serial.printf("[BOOT] MAC: %s | Channel: %d\n",
    WiFi.macAddress().c_str(), ESPNOW_CHANNEL);
  Serial.println("[BOOT] Ready. Sending first packet in 2s...");
  delay(2000);
}

// ─── Loop ─────────────────────────────────────────────────────────────────────
void loop() {
  unsigned long now = millis();

  if (now - lastSend >= SEND_INTERVAL_MS) {
    lastSend     = now;
    ackReceived  = false;
    retryCount   = 0;

    AQPacket pkt;
    build_packet(&pkt);
    send_packet(&pkt);

    // Wait for ACK with retries
    for (retryCount = 1; retryCount <= MAX_RETRIES; retryCount++) {
      unsigned long t = millis();
      while (millis() - t < RETRY_DELAY_MS) {
        yield(); // allow ESP-NOW callback to fire
      }
      if (ackReceived) break;

      Serial.printf("[RETRY] %d/%d for SEQ:%03d\n", retryCount, MAX_RETRIES, pkt.seq_num);
      send_packet(&pkt);
    }

    if (!ackReceived) {
      Serial.printf("[WARN] No ACK after %d retries for SEQ:%03d\n", MAX_RETRIES, pkt.seq_num);
    }

#if USE_OLED
    SensorPayload* sp = (SensorPayload*)pkt.payload;
    oled.clearDisplay();
    oled.setCursor(0, 0); oled.printf("Node 0x%02X SEQ:%03d", NODE_ID, pkt.seq_num);
    oled.setCursor(0,16); oled.printf("Temp: %.2f C", sp->temperature / 100.0f);
    oled.setCursor(0,26); oled.printf("Hum:  %.2f %%", sp->humidity / 100.0f);
    oled.setCursor(0,36); oled.printf("CO2:  %d ppm", sp->co2_ppm);
    oled.setCursor(0,46); oled.printf("AQI:  %d  Hop:%d", sp->air_quality, pkt.hop_count);
    oled.setCursor(0,56); oled.print(ackReceived ? "ACK OK" : "NO ACK");
    oled.display();
#endif
  }
}

/*
  relay_node.ino
  Secure Air Quality Mesh Cluster — Relay Node Firmware

  No sensors. Listens for AQPacket from sensor nodes,
  checks hop count, deduplicates, and forwards to coordinator.

  Can run on ESP32 or ESP8266.
*/

#include <esp_now.h>
#include <WiFi.h>
#include "config.h"
#include "../shared/packet_types.h"
#include "../shared/crc8.h"

// ─── Dedup cache ─────────────────────────────────────────────────────────────
struct DedupEntry {
  uint8_t src_id;
  uint8_t seq_num;
  bool    valid;
};

static DedupEntry dedupCache[DEDUP_CACHE_SIZE] = {};
static uint8_t    dedupIdx = 0;

bool is_duplicate(uint8_t src, uint8_t seq) {
  for (int i = 0; i < DEDUP_CACHE_SIZE; i++) {
    if (dedupCache[i].valid &&
        dedupCache[i].src_id  == src &&
        dedupCache[i].seq_num == seq) {
      return true;
    }
  }
  // Not found — add to cache
  dedupCache[dedupIdx % DEDUP_CACHE_SIZE] = { src, seq, true };
  dedupIdx++;
  return false;
}

// ─── Callback ─────────────────────────────────────────────────────────────────
void onDataRecv(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
  if (len != sizeof(AQPacket)) return;

  AQPacket pkt;
  memcpy(&pkt, data, sizeof(AQPacket));

  // Validate CRC
  uint8_t expected = crc8_compute((uint8_t*)&pkt, sizeof(AQPacket) - 1);
  if (pkt.crc8 != expected) {
    Serial.printf("[RELAY] CRC ERR from Node %02X — drop\n", pkt.source_id);
    return;
  }

  // Don't relay ACKs or packets destined for this relay node
  if (pkt.type == PKT_ACK)              return;
  if (pkt.source_id == RELAY_NODE_ID)   return;

  // Hop limit
  if (pkt.hop_count >= MAX_HOPS) {
    Serial.printf("[RELAY] Max hops reached (%d) — drop\n", pkt.hop_count);
    return;
  }

  // Dedup
  if (is_duplicate(pkt.source_id, pkt.seq_num)) {
    Serial.printf("[RELAY] Dup Node:%02X SEQ:%03d — drop\n", pkt.source_id, pkt.seq_num);
    return;
  }

  // Forward
  pkt.hop_count++;
  pkt.flags |= 0x02;  // Mark as relayed
  // Recompute CRC after modifying header
  pkt.crc8 = crc8_compute((uint8_t*)&pkt, sizeof(AQPacket) - 1);

  esp_err_t err = esp_now_send(COORDINATOR_MAC, (uint8_t*)&pkt, sizeof(AQPacket));
  Serial.printf("[RELAY] Forwarded Node:%02X SEQ:%03d Hop:%d → %s\n",
    pkt.source_id, pkt.seq_num, pkt.hop_count,
    err == ESP_OK ? "OK" : "FAIL");
}

void onDataSent(const uint8_t* mac, esp_now_send_status_t status) {
  // Optional: log forward delivery
}

// ─── Setup ────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  Serial.printf("\n[BOOT] Relay Node 0x%02X starting...\n", RELAY_NODE_ID);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  if (esp_now_init() != ESP_OK) {
    Serial.println("[ERR] ESP-NOW init failed");
    ESP.restart();
  }

  esp_now_register_recv_cb(onDataRecv);
  esp_now_register_send_cb(onDataSent);

  // Register coordinator as peer (forward target)
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, COORDINATOR_MAC, 6);
  peer.channel = ESPNOW_CHANNEL;
  peer.encrypt = false;
  if (esp_now_add_peer(&peer) != ESP_OK) {
    Serial.println("[ERR] Failed to add coordinator peer");
  }

  Serial.printf("[BOOT] MAC: %s | Channel: %d\n",
    WiFi.macAddress().c_str(), ESPNOW_CHANNEL);
  Serial.println("[BOOT] Relay ready — listening...");
}

void loop() {
  // Nothing — all work is in the ESP-NOW receive callback
  delay(10);
}

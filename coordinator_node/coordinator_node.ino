/*
  coordinator_node.ino
  Secure Air Quality Mesh Cluster — Coordinator Node Firmware

  Receives AQPackets from all sensor nodes (direct or via relay).
  Validates CRC-8, deduplicates, aggregates per-node data.
  Every POST_INTERVAL_MS: connects to Wi-Fi, POSTs JSON to Flask dashboard, disconnects.

  Hardware:
    ESP32 DevKit v1 (30-pin) — must be ESP32 (dual-mode: ESP-NOW + Wi-Fi STA)
*/

#include <esp_now.h>
#include <WiFi.h>
#include "config.h"
#include "../shared/packet_types.h"
#include "../shared/crc8.h"
#include "aggregator.h"
#include "wifi_uplink.h"

// ─── Dedup (coordinator-side) ─────────────────────────────────────────────────
#define COORD_DEDUP_SIZE 32
struct CoordDedup { uint8_t src; uint8_t seq; bool valid; };
static CoordDedup dedup[COORD_DEDUP_SIZE] = {};
static uint8_t    dedupIdx = 0;

bool is_duplicate(uint8_t src, uint8_t seq) {
  for (int i = 0; i < COORD_DEDUP_SIZE; i++) {
    if (dedup[i].valid && dedup[i].src == src && dedup[i].seq == seq) return true;
  }
  dedup[dedupIdx % COORD_DEDUP_SIZE] = { src, seq, true };
  dedupIdx++;
  return false;
}

// ─── ACK sender ───────────────────────────────────────────────────────────────
void send_ack(const uint8_t* dest_mac, uint8_t dest_id, uint8_t seq) {
  AQPacket ack = {};
  ack.version   = PKT_VERSION;
  ack.type      = PKT_ACK;
  ack.source_id = COORDINATOR_ID;
  ack.dest_id   = dest_id;
  ack.seq_num   = seq;
  ack.crc8      = crc8_compute((uint8_t*)&ack, sizeof(AQPacket) - 1);
  esp_now_send(dest_mac, (uint8_t*)&ack, sizeof(AQPacket));
}

// ─── ESP-NOW receive callback ─────────────────────────────────────────────────
void onDataRecv(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
  if (len != sizeof(AQPacket)) return;

  AQPacket pkt;
  memcpy(&pkt, data, len);

  // Validate CRC
  uint8_t expected = crc8_compute((uint8_t*)&pkt, sizeof(AQPacket) - 1);
  if (pkt.crc8 != expected) {
    Serial.printf("[CRC ERR] Node 0x%02X SEQ:%03d — drop\n", pkt.source_id, pkt.seq_num);
    return;
  }

  // Ignore non-data packets (other coordinators / stray traffic)
  if (pkt.type != PKT_DATA && pkt.type != PKT_HEARTBEAT) return;

  // Dedup
  if (is_duplicate(pkt.source_id, pkt.seq_num)) {
    Serial.printf("[DUP] Node 0x%02X SEQ:%03d — drop\n", pkt.source_id, pkt.seq_num);
    return;
  }

  int8_t rssi = info->rx_ctrl->rssi;
  Serial.printf("[RX] Node:0x%02X SEQ:%03d Hop:%d RSSI:%d CRC:OK%s\n",
    pkt.source_id, pkt.seq_num, pkt.hop_count, rssi,
    (pkt.flags & 0x02) ? " [RELAYED]" : "");

  aggregator_update(&pkt, rssi);

  // Send ACK back to sender's MAC
  if (pkt.flags & 0x01) {
    send_ack(info->src_addr, pkt.source_id, pkt.seq_num);
  }
}

// ─── Setup ────────────────────────────────────────────────────────────────────
static unsigned long lastPost = 0;

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n[BOOT] Coordinator starting...");

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  if (esp_now_init() != ESP_OK) {
    Serial.println("[ERR] ESP-NOW init failed");
    ESP.restart();
  }
  esp_now_register_recv_cb(onDataRecv);

  // Register all known nodes as peers
  for (int i = 0; i < KNOWN_NODE_COUNT; i++) {
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, KNOWN_NODES[i].mac, 6);
    peer.channel = ESPNOW_CHANNEL;
    peer.encrypt = false;
    if (esp_now_add_peer(&peer) == ESP_OK) {
      Serial.printf("[PEER] Registered Node 0x%02X\n", KNOWN_NODES[i].id);
    } else {
      Serial.printf("[PEER] Failed to register Node 0x%02X\n", KNOWN_NODES[i].id);
    }
  }

  aggregator_init();
  lastPost = millis();

  Serial.printf("[BOOT] MAC: %s | ESP-NOW Channel: %d\n",
    WiFi.macAddress().c_str(), ESPNOW_CHANNEL);
  Serial.println("[BOOT] Coordinator ready — waiting for packets...\n");
}

// ─── Loop ─────────────────────────────────────────────────────────────────────
void loop() {
  if (millis() - lastPost >= POST_INTERVAL_MS) {
    lastPost = millis();
    aggregator_print_table();
    String json = aggregator_to_json();
    Serial.println("[JSON] " + json);
    wifi_post(DASHBOARD_URL, json);
  }
  delay(10);
}

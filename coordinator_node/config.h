#pragma once

/*
  coordinator_node/config.h
  ✅ SAFE TO COMMIT — contains only placeholders.

  Credentials and real IPs live in config_local.h (gitignored).
  Copy config_local.h.example → config_local.h and fill it in before flashing.
*/

// ─── Load local secrets (gitignored) ─────────────────────────────────────────
#if __has_include("config_local.h")
  #include "config_local.h"
#endif

// ─── Node Identity ────────────────────────────────────────────────────────────
#define COORDINATOR_ID      0x00

// ─── ESP-NOW ──────────────────────────────────────────────────────────────────
#define ESPNOW_CHANNEL      1

// ─── Wi-Fi Uplink (placeholders — real values go in config_local.h) ──────────
#ifndef WIFI_SSID
#define WIFI_SSID           "YOUR_WIFI_SSID"
#endif

#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD       "YOUR_WIFI_PASSWORD"
#endif

#ifndef DASHBOARD_URL
#define DASHBOARD_URL       "http://YOUR_SERVER_IP:5000/api/data"
#endif

#define WIFI_TIMEOUT_MS     10000
#define POST_INTERVAL_MS    30000   // POST every 30 s

// ─── Known Sensor Nodes ───────────────────────────────────────────────────────
// Coordinator registers these as ESP-NOW peers so it can send ACKs.
// Relay packets still carry the original source_id — relays don't go here.
struct NodeDef {
  uint8_t id;
  uint8_t mac[6];
};

#define KNOWN_NODE_COUNT  1                              // <<< UPDATE after mac_scanner
static const NodeDef KNOWN_NODES[KNOWN_NODE_COUNT] = {
  { 0x01, { 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x02 } }   // <<< REPLACE with real MAC
};

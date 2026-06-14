#pragma once

/*
  relay_node/config.h
  Relay node only forwards packets — no sensors, no Wi-Fi uplink.

  Steps before flashing:
    1. Run mac_scanner.ino on coordinator → paste into COORDINATOR_MAC
    2. RELAY_NODE_ID must match what sensor nodes use in their RELAY_MAC
*/

// ─── Node Identity ────────────────────────────────────────────────────────────
#define RELAY_NODE_ID    0x10

// ─── ESP-NOW ──────────────────────────────────────────────────────────────────
#define ESPNOW_CHANNEL   1
#define DEDUP_CACHE_SIZE 16

// ─── Coordinator MAC ──────────────────────────────────────────────────────────
// Run mac_scanner.ino on your coordinator and paste its MAC here
uint8_t COORDINATOR_MAC[6] = { 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x01 };  // <<< REPLACE

#pragma once

/*
  sensor_node/config.h
  All tunable parameters for the sensor node firmware.

  Steps before flashing:
    1. Set NODE_ID to a unique value per node (0x01, 0x02, ... 0xFE)
    2. Run mac_scanner.ino on coordinator → paste into COORDINATOR_MAC
    3. If using a relay, set USE_RELAY true and fill RELAY_MAC
    4. Calibrate MQ135_CLEAN_AIR by reading analogRead(MQ135_PIN) in fresh air
*/

// ─── Node Identity ────────────────────────────────────────────────────────────
// Change this for every sensor node you flash — must be unique per deployment
#define NODE_ID             0x01                       // <<< UNIQUE PER NODE

// ─── Hardware Pins ────────────────────────────────────────────────────────────
#define DHT_PIN             4                          // GPIO4  → DHT22 data pin
#define DHT_TYPE            22                         // 22 = DHT22 / AM2302
#define MQ135_PIN           34                         // GPIO34 → MQ-135 AOUT (ADC1)

// MQ-135 linear calibration
// MQ135_CLEAN_AIR: analogRead value in fresh outdoor air (~400 ppm CO2)
//   Warm up sensor for 30 min, then call analogRead(MQ135_PIN) and paste here.
#define MQ135_CLEAN_AIR     1650                       // <<< CALIBRATE
#define MQ135_PPM_MIN       400                        // mapped output floor (ppm)
#define MQ135_PPM_MAX       5000                       // mapped output ceiling (ppm)

// ─── Optional OLED (SSD1306 128×64 I²C) ──────────────────────────────────────
#define USE_OLED            0                          // 1 = enable, 0 = disable
#define OLED_SDA            21
#define OLED_SCL            22

// ─── ESP-NOW ──────────────────────────────────────────────────────────────────
#define ESPNOW_CHANNEL      1
#define SEND_INTERVAL_MS    10000                      // Read + send every 10 s

// ─── Routing ──────────────────────────────────────────────────────────────────
// Set USE_RELAY to true if this node is out of direct ESP-NOW range of the
// coordinator. Packets will be sent to RELAY_MAC which forwards them onward.
#define USE_RELAY           false

// Run mac_scanner.ino on coordinator and relay, paste MACs below
uint8_t COORDINATOR_MAC[6] = { 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x01 };  // <<< REPLACE
uint8_t RELAY_MAC[6]       = { 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x10 };  // <<< REPLACE (USE_RELAY only)

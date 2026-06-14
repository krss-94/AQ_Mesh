#pragma once
#include <stdint.h>

// ─── Protocol Constants ───────────────────────────────────────────────────────
#define PKT_VERSION       0x01
#define PKT_DATA          0x01
#define PKT_ACK           0x02
#define PKT_HEARTBEAT     0x03

#define NODE_BROADCAST    0xFF
#define MAX_HOPS          4
#define MAX_RETRIES       3
#define RETRY_DELAY_MS    500

// ─── Sensor Payload (23 bytes) ────────────────────────────────────────────────
typedef struct __attribute__((packed)) {
  int16_t  temperature;    // °C × 100  (e.g. 2754 = 27.54°C)
  uint16_t humidity;       // %RH × 100 (e.g. 6523 = 65.23%)
  uint16_t pm25;           // µg/m³ × 10
  uint16_t co2_ppm;        // CO2 estimate from MQ-135
  uint16_t voc_raw;        // Raw ADC value (0–4095)
  uint8_t  air_quality;    // 0=Good 1=Moderate 2=USG 3=Unhealthy 4=VeryUnhealthy 5=Hazardous
  uint32_t timestamp_ms;   // millis() since node boot
  uint8_t  reserved[8];    // future use
} SensorPayload;            // = 23 bytes

// ─── Main Packet (32 bytes) ───────────────────────────────────────────────────
typedef struct __attribute__((packed)) {
  uint8_t  version;        // PKT_VERSION
  uint8_t  type;           // PKT_DATA / PKT_ACK / PKT_HEARTBEAT
  uint8_t  source_id;      // Sending node ID
  uint8_t  dest_id;        // 0xFF = coordinator
  uint8_t  seq_num;        // Rolling 0–255
  uint8_t  hop_count;      // Incremented by each relay
  uint8_t  flags;          // Bit0=ACK_REQ Bit1=RELAYED Bit2=ENCRYPTED
  uint8_t  payload_len;    // sizeof(SensorPayload) or 0 for ACK/HB
  uint8_t  payload[23];    // SensorPayload cast into here
  uint8_t  crc8;           // CRC-8 over bytes 0–30
} AQPacket;                 // = 32 bytes

static_assert(sizeof(SensorPayload) == 23, "SensorPayload must be 23 bytes");
static_assert(sizeof(AQPacket)      == 32, "AQPacket must be 32 bytes");

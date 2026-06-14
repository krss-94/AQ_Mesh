#include "aggregator.h"
#include "config.h"
#include <Arduino.h>

/*
  aggregator.cpp
  Tracks per-node state on the coordinator.

  - aggregator_init()         : zero the table
  - aggregator_update()       : upsert on each received packet
  - aggregator_to_json()      : serialize all active nodes → JSON string for Flask
  - aggregator_print_table()  : pretty-print table to Serial on each POST cycle
*/

// ─── Internal state ───────────────────────────────────────────────────────────
#define MAX_NODES        16
#define NODE_TIMEOUT_MS  120000   // 2 min without a packet → mark stale

static NodeRecord nodes[MAX_NODES];

// ─── Init ─────────────────────────────────────────────────────────────────────
void aggregator_init() {
  memset(nodes, 0, sizeof(nodes));
  Serial.println("[AGG] Node table initialized (capacity: " + String(MAX_NODES) + ")");
}

// ─── Internal: find existing record or claim an empty slot ───────────────────
static NodeRecord* find_or_create(uint8_t node_id) {
  // Search for existing
  for (int i = 0; i < MAX_NODES; i++) {
    if (nodes[i].active && nodes[i].node_id == node_id) return &nodes[i];
  }
  // Claim first empty slot
  for (int i = 0; i < MAX_NODES; i++) {
    if (!nodes[i].active) {
      memset(&nodes[i], 0, sizeof(NodeRecord));
      nodes[i].active  = true;
      nodes[i].node_id = node_id;
      Serial.printf("[AGG] New node registered: 0x%02X (slot %d)\n", node_id, i);
      return &nodes[i];
    }
  }
  return nullptr;   // table full
}

// ─── Update ───────────────────────────────────────────────────────────────────
void aggregator_update(const AQPacket* pkt, int8_t rssi) {
  if (!pkt) return;
  // Only store DATA payloads; heartbeats update presence but not readings
  NodeRecord* r = find_or_create(pkt->source_id);
  if (!r) {
    Serial.println("[AGG] Table full — cannot register new node 0x"
                   + String(pkt->source_id, HEX));
    return;
  }

  if (pkt->type == PKT_DATA && pkt->payload_len == sizeof(SensorPayload)) {
    memcpy(&r->last_reading, pkt->payload, sizeof(SensorPayload));
  }

  r->rssi         = rssi;
  r->hop_count    = pkt->hop_count;
  r->last_seen_ms = millis();
  r->packet_count++;
}

// ─── JSON serializer ──────────────────────────────────────────────────────────
String aggregator_to_json() {
  uint32_t now   = millis();
  String   json  = "{\"uptime_s\":" + String(now / 1000) + ",\"nodes\":[";
  bool     first = true;

  for (int i = 0; i < MAX_NODES; i++) {
    if (!nodes[i].active) continue;

    SensorPayload* sp    = &nodes[i].last_reading;
    uint32_t       age_s = (now - nodes[i].last_seen_ms) / 1000;
    bool           stale = (now - nodes[i].last_seen_ms) > NODE_TIMEOUT_MS;

    if (!first) json += ",";
    first = false;

    char buf[320];
    snprintf(buf, sizeof(buf),
      "{"
        "\"id\":\"0x%02X\","
        "\"temp_c\":%.2f,"
        "\"humidity_pct\":%.2f,"
        "\"co2_ppm\":%u,"
        "\"voc_raw\":%u,"
        "\"pm25_x10\":%u,"
        "\"aqi\":%u,"
        "\"rssi_dbm\":%d,"
        "\"hops\":%u,"
        "\"pkt_count\":%u,"
        "\"age_s\":%u,"
        "\"stale\":%s"
      "}",
      nodes[i].node_id,
      sp->temperature / 100.0f,
      sp->humidity    / 100.0f,
      (unsigned)sp->co2_ppm,
      (unsigned)sp->voc_raw,
      (unsigned)sp->pm25,
      (unsigned)sp->air_quality,
      (int)nodes[i].rssi,
      (unsigned)nodes[i].hop_count,
      (unsigned)nodes[i].packet_count,
      (unsigned)age_s,
      stale ? "true" : "false"
    );
    json += buf;
  }

  json += "]}";
  return json;
}

// ─── Serial table ─────────────────────────────────────────────────────────────
static const char* AQI_LABELS[] = {
  "GOOD ", "MOD  ", "USG  ", "UNHLT", "V-UNH", "HAZRD"
};

void aggregator_print_table() {
  uint32_t now = millis();
  Serial.println();
  Serial.println(F("=========================================================================="));
  Serial.printf (  "  COORDINATOR NODE TABLE  |  uptime: %lus\n", (unsigned long)(now / 1000));
  Serial.println(F("--------------------------------------------------------------------------"));
  Serial.println(F("  ID    Temp     Hum    CO2     AQI    Hops  RSSI    Pkts   Age   Status"));
  Serial.println(F("--------------------------------------------------------------------------"));

  bool any = false;
  for (int i = 0; i < MAX_NODES; i++) {
    if (!nodes[i].active) continue;
    any = true;

    SensorPayload* sp    = &nodes[i].last_reading;
    uint32_t       age_s = (now - nodes[i].last_seen_ms) / 1000;
    bool           stale = (now - nodes[i].last_seen_ms) > NODE_TIMEOUT_MS;
    const char*    aqi   = (sp->air_quality <= 5) ? AQI_LABELS[sp->air_quality] : "?????";

    char row[128];
    snprintf(row, sizeof(row),
      "  0x%02X  %5.1fC  %5.1f%%  %4uppm  [%s]  %d     %4ddBm  %4u   %3us  %s",
      nodes[i].node_id,
      sp->temperature / 100.0f,
      sp->humidity    / 100.0f,
      (unsigned)sp->co2_ppm,
      aqi,
      (unsigned)nodes[i].hop_count,
      (int)nodes[i].rssi,
      (unsigned)nodes[i].packet_count,
      (unsigned)age_s,
      stale ? "STALE" : "OK"
    );
    Serial.println(row);
  }

  if (!any) {
    Serial.println(F("  (no nodes seen yet)"));
  }

  Serial.println(F("=========================================================================="));
  Serial.println();
}

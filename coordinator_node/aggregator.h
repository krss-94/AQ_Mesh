#pragma once
#include "../shared/packet_types.h"
#include <Arduino.h>

struct NodeRecord {
  uint8_t       node_id;
  SensorPayload last_reading;
  int8_t        rssi;
  uint8_t       hop_count;
  uint32_t      last_seen_ms;
  uint32_t      packet_count;
  bool          active;
};

void   aggregator_init();
void   aggregator_update(const AQPacket* pkt, int8_t rssi);
String aggregator_to_json();
void   aggregator_print_table();

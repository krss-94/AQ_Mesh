#pragma once
#include "../shared/packet_types.h"

void sensors_init();
void sensors_read(SensorPayload* out);
uint8_t compute_aqi(uint16_t co2_ppm, uint16_t pm25_x10);

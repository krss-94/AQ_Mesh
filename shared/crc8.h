#pragma once
#include <stdint.h>

// CRC-8/MAXIM polynomial 0x31
inline uint8_t crc8_compute(const uint8_t* data, uint8_t len) {
  uint8_t crc = 0x00;
  for (uint8_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (uint8_t b = 0; b < 8; b++) {
      if (crc & 0x80) crc = (crc << 1) ^ 0x31;
      else            crc <<= 1;
    }
  }
  return crc;
}

inline bool crc8_verify(const uint8_t* data, uint8_t len, uint8_t expected) {
  return crc8_compute(data, len) == expected;
}

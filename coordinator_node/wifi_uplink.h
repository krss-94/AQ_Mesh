#pragma once
#include <Arduino.h>

/*
  wifi_uplink.h
  Connects to Wi-Fi, POSTs JSON to Flask dashboard, disconnects.
  Defined in wifi_uplink.cpp.

  Usage:
    wifi_post(DASHBOARD_URL, json_string);

  Wi-Fi credentials and URL live in coordinator_node/config.h.
*/

void wifi_post(const char* url, const String& json);

<div align="center">

<br/>

```
 █████╗  ██████╗      ███╗   ███╗███████╗███████╗██╗  ██╗
██╔══██╗██╔═══██╗     ████╗ ████║██╔════╝██╔════╝██║  ██║
███████║██║   ██║     ██╔████╔██║█████╗  ███████╗███████║
██╔══██║██║▄▄ ██║     ██║╚██╔╝██║██╔══╝  ╚════██║██╔══██║
██║  ██║╚██████╔╝     ██║ ╚═╝ ██║███████╗███████║██║  ██║
╚═╝  ╚═╝ ╚══▀▀═╝      ╚═╝     ╚═╝╚══════╝╚══════╝╚═╝  ╚═╝
```

**Distributed Air Quality Monitoring over ESP-NOW Mesh**

[![Platform](https://img.shields.io/badge/platform-ESP32-E7352C?logo=espressif&logoColor=white)](https://www.espressif.com/)
[![Language](https://img.shields.io/badge/language-C%2B%2B-00599C?logo=cplusplus&logoColor=white)](https://isocpp.org/)
[![Protocol](https://img.shields.io/badge/wireless-ESP--NOW-F5A623?logoColor=white)]()
[![Packet](https://img.shields.io/badge/packet-32%20bytes%20fixed-8B5CF6)]()
[![CRC](https://img.shields.io/badge/integrity-CRC--8%2FMAXIM-22c55e)]()
[![License](https://img.shields.io/badge/license-MIT-22c55e)](LICENSE)
[![Status](https://img.shields.io/badge/status-in%20development-F59E0B)]()

<br/>

> Multi-node ESP32 mesh that samples temperature, humidity, CO₂ and VOC at the edge,  
> forwards encrypted packets over ESP-NOW with CRC-8 validation and hop counting,  
> and delivers aggregated JSON to a Flask dashboard over Wi-Fi — **no router between nodes.**

<br/>

</div>

---

## Network Topology

```
  ┌───────────────┐  ESP-NOW   ┌───────────────┐  ESP-NOW   ┌──────────────────┐
  │  Sensor 0x01  │──────────▶ │  Relay  0x10  │──────────▶ │  Coordinator     │
  │  DHT22+MQ135  │            │  (forwarder)  │            │  0x00            │
  └───────────────┘            └───────────────┘            │  Aggregator      │──▶ Flask :5000
                                                            │  Wi-Fi uplink    │    JSON POST
  ┌───────────────┐  ESP-NOW (direct, if in range)          └──────────────────┘
  │  Sensor 0x02  │────────────────────────────────────────▶        ▲
  │  DHT22+MQ135  │                                                  │
  └───────────────┘                                        Sends ACK back to src
```

Each packet carries `source_id`, `hop_count`, and a rolling `seq_num`.  
The relay increments `hop_count`, sets `RELAYED` flag, deduplicates by `(src, seq)`, and forwards.  
The coordinator aggregates per-node and POSTs JSON every 30 s.

---

## Packet Format — 32 bytes fixed

```
Byte:  0      1      2      3      4      5      6      7
      ┌──────┬──────┬──────┬──────┬──────┬──────┬──────┬──────┐
      │ VER  │ TYPE │ SRC  │ DST  │ SEQ  │ HOP  │FLAGS │ LEN  │
      └──────┴──────┴──────┴──────┴──────┴──────┴──────┴──────┘

Byte:  8 ──────────────────────────────────────────────── 30
      ┌────────────────────────────────────────────────────────┐
      │                   PAYLOAD  (23 bytes)                  │
      │  int16 temp │ uint16 hum │ pm25 │ co2 │ voc │ aqi │ts │
      └────────────────────────────────────────────────────────┘

Byte:  31
      ┌──────┐
      │ CRC8 │  ← CRC-8/MAXIM over bytes 0–30
      └──────┘

FLAGS:  bit0 = ACK_REQ  │  bit1 = RELAYED  │  bit2 = ENCRYPTED (future)
TYPES:  0x01 DATA       │  0x02 ACK        │  0x03 HEARTBEAT
```

---

## Hardware

| Role        | MCU             | Sensors              | Range     |
|-------------|-----------------|----------------------|-----------|
| Sensor Node | ESP32 DevKit v1 | DHT22 + MQ-135       | ~200 m LOS|
| Relay Node  | ESP32 DevKit v1 | —                    | extends mesh|
| Coordinator | ESP32 DevKit v1 | — (Wi-Fi uplink)     | fixed     |

**Sensor Node Wiring**

```
ESP32 GPIO4  ──[10kΩ pull-up to 3.3V]── DHT22 DATA
ESP32 GPIO34 ────────────────────────── MQ-135 AOUT   (ADC1 — never use ADC2 with Wi-Fi)
ESP32 Vin    ────────────────────────── MQ-135 VCC    (5V)
ESP32 GND    ────────────────────────── DHT22 GND / MQ-135 GND

Optional OLED (SSD1306 I²C):
ESP32 GPIO21 ── SDA
ESP32 GPIO22 ── SCL
```

> ⚠️ MQ-135 needs **30 min warm-up** before CO₂ readings are reliable.  
> ADC2 pins (GPIO 0, 2, 4, 12–15, 25–27) are **unusable** when Wi-Fi is active on the coordinator.

---

## Repository Structure

```
AQ_Mesh/
│
├── shared/                         # Included by all three node types
│   ├── packet_types.h              # AQPacket (32B) + SensorPayload (23B)
│   └── crc8.h                      # CRC-8/MAXIM, inline, no dependency
│
├── sensor_node/
│   ├── sensor_node.ino             # Main sketch — ESP-NOW send loop
│   ├── sensors.h / sensors.cpp     # DHT22 + MQ-135 abstraction layer
│   └── config.h                    # ← edit NODE_ID + COORDINATOR_MAC
│
├── coordinator_node/
│   ├── coordinator_node.ino        # Main sketch — receive, ACK, uplink
│   ├── aggregator.h / .cpp         # Per-node state table (16 slots)
│   ├── wifi_uplink.h / .cpp        # Connect → POST JSON → disconnect
│   ├── config.h                    # ← edit KNOWN_NODES[] + DASHBOARD_URL
│   └── config_local.h.example      # Copy → config_local.h, fill credentials
│
├── relay_node/
│   ├── relay_node.ino              # Dedup cache + forward with hop++
│   └── config.h                    # ← edit COORDINATOR_MAC
│
├── mac_scanner/
│   └── mac_scanner.ino             # Flash first — prints MAC in config.h format
│
├── docs/
│   └── index.html                  # GitHub Pages project site
│
└── .gitignore                      # config_local.h is blocked — never commit credentials
```

---

## Quick Start

### 0 — Prerequisites

```bash
# Arduino IDE 2.x
# Board package: esp32 by Espressif (≥ 3.0.0)
# Libraries (Library Manager):
#   DHT sensor library        — Adafruit
#   Adafruit Unified Sensor   — Adafruit
#   Adafruit SSD1306          — Adafruit  (only if USE_OLED 1)
```

### 1 — Get MAC addresses

Flash `mac_scanner/mac_scanner.ino` onto **each** ESP32, open Serial Monitor at **115200 baud**, copy the array output.

```
==============================
       ESP32 MAC SCANNER
==============================
Array format:
{ 0x24, 0x6F, 0x28, 0xAB, 0xCD, 0xEF }
```

### 2 — Configure sensor node

Edit `sensor_node/config.h`:

```cpp
#define NODE_ID          0x01       // unique per node: 0x01, 0x02 ...
uint8_t COORDINATOR_MAC[6] = { 0x24, 0x6F, ... };   // coordinator's MAC
```

### 3 — Configure coordinator

Copy and fill credentials locally (never committed):

```bash
cp coordinator_node/config_local.h.example coordinator_node/config_local.h
# edit: WIFI_SSID, WIFI_PASSWORD, DASHBOARD_URL
```

Edit `coordinator_node/config.h` — add each sensor's MAC:

```cpp
#define KNOWN_NODE_COUNT  1
static const NodeDef KNOWN_NODES[] = {
  { 0x01, { 0x24, 0x6F, ... } },
};
```

### 4 — MQ-135 calibration

Power on sensor node outdoors (or near an open window).  
Wait **30 minutes**, open Serial Monitor, note the `voc_raw` value.  
Set `MQ135_CLEAN_AIR` in `sensor_node/config.h` to that value.

### 5 — Flash order

```
1. Coordinator   ← must be listening first
2. Sensor node(s)
3. Relay node(s) ← only if sensor is out of direct range
```

### 6 — Verify

Coordinator Serial Monitor should show:

```
[RX] Node:0x01  SEQ:000  Hop:0  RSSI:-52dBm  CRC:OK
[SENSOR] Temp:27.54°C  Hum:65.23%  CO2:612ppm  AQI:1
```

That's your `v0.1`. Commit it.

---

## Milestones

| Tag | What works |
|-----|-----------|
| `v0.1-espnow-ok` | `Sensor → Coordinator` · `[RX] ... CRC:OK` on Serial |
| `v0.2-relay-ok`  | `Sensor → Relay → Coordinator` · `RELAYED` flag set |
| `v0.3-uplink-ok` | Coordinator POSTs JSON · Flask returns `200` |
| `v1.0-dashboard` | Live data on web dashboard |

---

## Credential Safety

`config_local.h` is blocked by `.gitignore`. Before every push, verify:

```bash
git status   # config_local.h must NOT appear
```

If it appears:
```bash
git rm --cached coordinator_node/config_local.h
echo "coordinator_node/config_local.h" >> .gitignore
```

Never commit SSID, passwords, API keys, or tokens to a public repo. Ever.

---

## License

MIT — see [LICENSE](LICENSE).  
Built at **Sathyabama Institute of Science and Technology**, Dept. of ECE.


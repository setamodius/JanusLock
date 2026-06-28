# Janus Lock

MQTT-controlled garage door automation using an ESP32 and an existing rolling-code remote control.

Janus Lock does **not** attempt to clone, decode, or bypass the RF protocol. Instead, it electronically simulates a button press on the original remote, preserving the security and reliability of the existing system.

## Features

* MQTT-based control
* Supports existing rolling-code remotes
* ESP32-C3 OLED based
* Web-based control UI and configuration
* WiFi provisioning via Access Point mode
* No battery required for the remote
* Compact and low-cost design

## Hardware

| Component | Details |
|-----------|---------|
| Microcontroller | ESP32-C3 DevKitM-1 |
| Display | SSD1306 72×40 OLED (I2C) |
| Relay | CPC1017N Solid State Relay |
| Boost Converter | XL6009 |
| Remote | Original Garage Remote |
| Power | USB-C Power Supply |

**Pin mapping:**

| Signal | Pin |
|--------|-----|
| OLED SDA | 5 |
| OLED SCL | 6 |
| Door 1 relay | 4 |
| Door 2 relay | 7 |
| LED | 8 |
| Reset button | 9 |

## Architecture

```
MQTT Broker
     |
     v
ESP32-C3  ←→  Web UI (Node.js + Browser)
     |
     v
CPC1017N
     |
     v
Garage Remote
     |
     v
Garage Door
```

## Power Design

```
USB-C 5V
    |
    +--> ESP32-C3
    |
    +--> XL6009
             |
             +--> 12V Remote Control
```

## MQTT Topics

All topics are prefixed with the configured `topicPrefix` (default: `keremhome`).

| Topic | Direction | Payload |
|-------|-----------|---------|
| `{prefix}/device/status` | Firmware → | `online` / `offline` (LWT) |
| `{prefix}/door1/status`  | Firmware → | `ready` / `triggered` |
| `{prefix}/door2/status`  | Firmware → | `ready` / `triggered` |
| `{prefix}/door1/command` | → Firmware | `open` / `press` / `trigger` / `1` |
| `{prefix}/door2/command` | → Firmware | `open` / `press` / `trigger` / `1` |

## Firmware

Built with [PlatformIO](https://platformio.org/).

**Dependencies** (auto-installed):
- `olikraus/U8g2`
- `tzapu/WiFiManager`
- `bblanchon/ArduinoJson`
- `knolleary/PubSubClient`

## Configuration

On first boot, Janus starts in Access Point mode.

Default configuration portal:

```
SSID: Janus-Setup
IP:   192.168.4.1
```

Configuration parameters:

* WiFi SSID / Password
* MQTT Host, Port, Username, Password
* Topic Prefix

**Factory reset:** Hold the BOOT button for 5 seconds.

## Web UI

Node.js + Express server serving the control UI. Connects to MQTT directly from the browser over WebSocket for real-time status.

```
cd web
cp .env.example .env
# Edit .env with your MQTT broker details
docker compose up -d
```

Available on port `9494`.

## Why Janus?

Most DIY garage door projects focus on RF cloning or protocol reverse engineering.

Janus takes a different approach:

* Keep the original remote.
* Keep the original security.
* Automate the button press.
* Keep it simple.

## Design Philosophy

> Simple is the Best

If a feature increases complexity without significantly improving reliability, it probably does not belong in Janus.

## Status

🚧 Prototype working

* MQTT communication operational
* Remote triggering operational
* OLED interface operational
* Web UI operational
* Enclosure in progress
* Long-term stability testing in progress

## License

MIT License

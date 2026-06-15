# Janus Lock

MQTT-controlled garage door automation using an ESP32 and an existing rolling-code remote control.

Janus Lock does **not** attempt to clone, decode, or bypass the RF protocol. Instead, it electronically simulates a button press on the original remote, preserving the security and reliability of the existing system.

## Features

* MQTT-based control
* Supports existing rolling-code remotes
* ESP32-C3 OLED based
* Web-based configuration
* WiFi provisioning via Access Point mode
* No battery required for the remote
* Compact and low-cost design

## Hardware

* ESP32-C3 OLED
* CPC1017N Solid State Relay
* XL6009 Boost Converter
* Original Garage Remote
* USB-C Power Supply

## Architecture

```text
MQTT Broker
     |
     v
ESP32-C3
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

```text
USB-C 5V
    |
    +--> ESP32-C3
    |
    +--> XL6009
             |
             +--> 12V Remote Control
```

## MQTT Topics

```text
<prefix>/door1/command
<prefix>/door1/status

<prefix>/door2/command
<prefix>/door2/status
```

Example:

```text
janus/door1/command
```

## Configuration

On first boot, Janus starts in Access Point mode.

Default configuration portal:

```text
SSID: Janus-Setup
IP:   192.168.4.1
```

Configuration parameters:

* WiFi SSID
* WiFi Password
* MQTT Server
* MQTT Port
* MQTT Username
* MQTT Password
* Topic Prefix

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
* Enclosure in progress
* Long-term stability testing in progress

## License

MIT License

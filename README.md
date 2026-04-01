# Oref Relay ESP32

ESP32 sketch that polls the Home Front Command alert feed and drives a relay for a selected alert area.

## Target Platform

- Arduino framework
- ESP32 boards
- Config comments mention ESP32-C3 pin defaults and Wi-Fi tuning

## Main Files

- `OrefRelayESP32.ino`: main application logic, Wi-Fi handling, polling, JSON parsing, relay control, LEDs, and web server loop
- `web_ui.cpp`: local web UI routes and HTML for settings pages
- `web_ui.h`: shared runtime config and web UI context structs
- `config.h`: default values, pin assignments, and timing constants

## Dependencies Used In Code

- `WiFi`
- `WiFiClientSecure`
- `WebServer`
- `Preferences`
- `ArduinoJson`
- `esp_wifi`

## Hardware Defaults

- relay pin: `GPIO 5`
- red LED: `GPIO 1`
- green LED: `GPIO 0`

## What It Does

- polls the Oref realtime and history alert endpoints over HTTPS
- matches alerts by configured area name
- turns the relay on for alert categories `1` and `2`
- turns the relay off for category `13`
- can also turn the relay on for category `14` when pre-alert mode is enabled
- exposes a local web UI for Wi-Fi and alert settings

## Configuration

Runtime settings are stored in ESP32 `Preferences` under `oref-relay`:

- alert area
- partial matching flag
- pre-alert flag
- Wi-Fi SSID
- Wi-Fi password

If saved Wi-Fi cannot connect within the configured timeout, the sketch opens a setup AP:

- SSID: `OrefRelaySetup`
- password: open network by default

The web UI is served on the device IP when connected to Wi-Fi, or on the AP IP when the setup AP is active.

## Basic Flashing And Use

1. Open the project in Arduino IDE or another Arduino-compatible ESP32 environment.
2. Select an ESP32 board and the correct port.
3. Install the required libraries if they are not already available.
4. Build and upload the sketch.
5. Open serial at `115200` if you want logs.
6. After boot:
   - if saved Wi-Fi works, open `http://<device-ip>/`
   - if not, connect to `OrefRelaySetup` and open `http://192.168.4.1/`
7. Set the alert area and Wi-Fi details in the web UI.

## Notes

- the code uses `setInsecure()` for HTTPS connections
- serial logging is controlled by `OREF_SERIAL_LOGGING`
- the default area constant in `config.h` is `רחובות`

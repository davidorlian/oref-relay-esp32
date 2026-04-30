# Oref Relay ESP32

ESP32 relay-control prototype for Home Front Command alert monitoring.

This repository contains an ESP32 Arduino sketch that polls alert feeds, matches alerts against a configured area, and drives a relay according to alert categories.  
The device also provides a local Web UI for Wi-Fi and alert configuration, with runtime settings stored in ESP32 `Preferences`.

> This is a prototype project. It is not an official alerting system, safety device, or replacement for certified emergency-warning channels.

---

## What It Does

The firmware:

- connects to Wi-Fi using saved credentials
- opens a setup access point if Wi-Fi is not configured or cannot connect
- polls realtime and history alert feeds over HTTPS
- parses JSON alert records
- matches alerts against a configured area name
- controls a relay according to the detected alert category
- serves a local Web UI for settings
- stores runtime configuration in ESP32 `Preferences`
- uses status LEDs to indicate Wi-Fi / runtime state

Basic runtime flow:

```text
Home Front Command alert feed
        |
        | HTTPS polling
        v
ESP32 firmware
        |
        | JSON parsing + area/category matching
        v
Relay output
```

---

## Repository Structure

```text
oref-relay-esp32/
├── OrefRelayESP32.ino
├── web_ui.cpp
├── web_ui.h
├── config.h
├── .gitignore
└── README.md
```

Main files:

| File | Role |
|---|---|
| `OrefRelayESP32.ino` | Main application logic: Wi-Fi, polling, JSON parsing, relay control, LEDs, and web server loop |
| `web_ui.cpp` | Local Web UI routes and HTML for settings pages |
| `web_ui.h` | Shared runtime configuration and Web UI context structs |
| `config.h` | Default values, pin assignments, timing constants, and logging settings |

---

## Target Platform

- ESP32 board
- Arduino framework
- Local Wi-Fi network
- Relay module connected to the configured relay pin

The configuration comments mention ESP32-C3 pin defaults and Wi-Fi tuning.

---

## Hardware Defaults

Default pins are defined in `config.h`:

| Function | Default |
|---|---|
| Relay output | `GPIO 5` |
| Red status LED | `GPIO 1` |
| Green status LED | `GPIO 0` |

---

## Runtime Configuration

Settings are stored in ESP32 `Preferences` under:

```text
oref-relay
```

Stored runtime settings include:

- alert area
- partial area matching flag
- pre-alert handling flag
- Wi-Fi SSID
- Wi-Fi password

Default area in `config.h`:

```text
רחובות
```

---

## Wi-Fi and Setup Portal

On boot, the firmware tries to connect using saved Wi-Fi credentials.

If saved Wi-Fi is missing or connection fails, the device opens a setup access point:

| Setting | Value |
|---|---|
| AP SSID | `OrefRelaySetup` |
| AP password | Open network by default |
| Default AP address | `http://192.168.4.1/` |

When connected to a local Wi-Fi network, the Web UI is available at the device IP address.

---

## Alert Handling

The firmware polls two endpoints:

- realtime alerts
- alert history

The code classifies alert records and applies relay behavior based on the resulting category.

Current behavior:

| Category | Meaning in firmware | Relay behavior |
|---|---|---|
| `1` | alert | ON |
| `2` | UAV alert | ON |
| `13` | end / update-clear behavior | OFF |
| `14` | pre-alert | ON only if pre-alert mode is enabled |

The firmware also avoids repeated relay changes for duplicate alert signatures, so the relay does not chatter on repeated polling of the same event.

---

## Web UI

The local Web UI allows configuring:

- alert area
- full or partial area matching
- pre-alert behavior
- Wi-Fi SSID
- Wi-Fi password
- Wi-Fi reset / recovery

Routes used by the firmware include:

| Route | Purpose |
|---|---|
| `/` | Main settings page |
| `/wifi` | Wi-Fi settings page |
| `/save` | Save settings |
| `/wifi-reset` | Clear Wi-Fi settings and restart setup portal |

---

## Dependencies

The sketch uses:

- `WiFi`
- `WiFiClientSecure`
- `WebServer`
- `Preferences`
- `ArduinoJson`
- `esp_wifi`

Serial logging is controlled by:

```cpp
OREF_SERIAL_LOGGING
```

---

## Basic Flashing and Use

1. Open the project in Arduino IDE or another Arduino-compatible ESP32 environment.
2. Select the correct ESP32 board and port.
3. Install the required libraries.
4. Build and upload the sketch.
5. Open Serial Monitor at `115200` baud if runtime logs are needed.
6. After boot:
   - if saved Wi-Fi works, open the device IP in a browser
   - if Wi-Fi setup is needed, connect to `OrefRelaySetup` and open `http://192.168.4.1/`
7. Configure the alert area and Wi-Fi details in the Web UI.

---

## Notes

- The code currently uses `setInsecure()` for HTTPS connections.
- Realtime polling and history polling use separate intervals.
- The firmware keeps serving the Web UI while polling and while waiting on network activity.
- The relay output is initialized to idle at boot before alert polling begins.

---

## Safety / Disclaimer

This repository documents a prototype relay-control project.

It is not an official Home Front Command product, certified alerting device, or life-safety system.  
Do not rely on this project as the only source of emergency alerts.

Relay-controlled hardware can affect real electrical devices. Use proper isolation, rated components, enclosure protection, and review by a qualified person before connecting the output to any real load.

---

## Author

David Orlian

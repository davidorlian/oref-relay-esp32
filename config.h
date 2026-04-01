#pragma once

// Default values used the first time the device boots or after config reset.
constexpr char OREF_DEFAULT_AREA[] = "\xD7\xA8\xD7\x97\xD7\x95\xD7\x91\xD7\x95\xD7\xAA";
constexpr int OREF_DEFAULT_RELAY_PIN = 5;
constexpr int OREF_RED_LED_PIN = 1;
constexpr int OREF_GREEN_LED_PIN = 0;
constexpr bool OREF_DEFAULT_ALLOW_PARTIAL_AREA_MATCH = false;
constexpr bool OREF_DEFAULT_TRIGGER_ON_PRE_ALERT = true;

// Recovery AP settings for the built-in config portal.
constexpr char OREF_CONFIG_AP_NAME[] = "OrefRelaySetup";
constexpr char OREF_CONFIG_AP_PASSWORD[] = "";
constexpr unsigned long OREF_CONFIG_PORTAL_TIMEOUT_S = 180;
constexpr char OREF_HOSTNAME[] = "oref-relay";

// Polling and network behavior.
constexpr unsigned long OREF_REALTIME_POLL_MS = 2000;
constexpr unsigned long OREF_HISTORY_POLL_MS = 30000;
constexpr unsigned long OREF_HTTP_TIMEOUT_MS = 8000;
constexpr unsigned long OREF_WIFI_CONNECT_TIMEOUT_MS = 20000;
constexpr unsigned long OREF_WIFI_PORTAL_AFTER_DISCONNECT_MS = 180000;
constexpr size_t OREF_JSON_DOC_BYTES = 8192;

// Wi-Fi power tuning for the ESP32-C3 Super Mini.
constexpr int8_t OREF_WIFI_TX_POWER_QUARTER_DBM = 40;

// Serial logs are useful during setup and troubleshooting.
constexpr bool OREF_SERIAL_LOGGING = true;

constexpr char OREF_USER_AGENT[] = "ESP32-OrefRelay/1.0";


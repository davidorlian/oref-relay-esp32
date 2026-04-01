#pragma once

#include <Arduino.h>
#include <WebServer.h>

#include "config.h"

struct RuntimeConfig {
  String area = OREF_DEFAULT_AREA;
  int relayPin = OREF_DEFAULT_RELAY_PIN;
  bool partial = OREF_DEFAULT_ALLOW_PARTIAL_AREA_MATCH;
  bool preAlert = OREF_DEFAULT_TRIGGER_ON_PRE_ALERT;
  String wifiSsid;
  String wifiPassword;
};

struct WebUiContext {
  WebServer *server;
  RuntimeConfig *config;
  bool *runtimeConfigPortalActive;
  void (*saveRuntimeConfig)();
  void (*retryWifiNow)();
  void (*restartRuntimeConfigPortal)();
  void (*logLine)(const String &);
  String (*normalizeAreaValue)(const String &);
};

void webUiBegin(WebUiContext &context);

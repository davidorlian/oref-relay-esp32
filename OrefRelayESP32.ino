#include <WiFi.h>
#include "esp_wifi.h"
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <WebServer.h>
#include "web_ui.h"
#include "config.h"

namespace {
constexpr char kOrefHost[] = "www.oref.org.il";
constexpr char kRealtimePath[] = "/warningMessages/alert/Alerts.json";
constexpr char kHistoryPath[] = "/warningMessages/alert/History/AlertsHistory.json";
constexpr char kOrefReferer[] = "https://www.oref.org.il/heb/alerts-history";
constexpr unsigned long kWifiRetryWhilePortalMs = 10000;

Preferences prefs;
WebServer server(80);
unsigned long lastHistoryPollMs = 0;
unsigned long lastRealtimePollMs = 0;
String lastHistorySignature;
String lastRealtimeSignature;
String lastAppliedLogicalSignature;
String lastHistoryInfoSignature;
String lastRealtimeInfoSignature;
String lastHistoryNoMatchSignature;
String lastRealtimeNoMatchSignature;
RuntimeConfig config;
bool wifiPortalOpenedForCurrentDisconnect = false;
bool runtimeConfigPortalActive = false;
bool webUiConfigured = false;
bool wifiStationHasIp = false;
bool historyFetchFailureLogged = false;
bool realtimeFetchFailureLogged = false;
bool startupRecoveryPending = false;
uint8_t consecutiveHistoryFetchFailures = 0;
uint8_t consecutiveRealtimeFetchFailures = 0;
unsigned long lastWifiRetryWhilePortalMs = 0;

String trimCopy(String value);
String normalizeAreaValue(const String &value);
void saveRuntimeConfig();
void logLine(const String &line);
void startWebUi();
void startRuntimeConfigPortal();
void stopRuntimeConfigPortal();
void restartRuntimeConfigPortal();
void retryWifiNow();

String decodeHtmlEntities(const String &value);
String readHttpLine(WiFiClientSecure &client);
bool pollEndpoint(const char *host, const char *path, const char *referer, bool startupRecovery, const char *sourceLabel);
bool readNextJsonObject(WiFiClientSecure &client, String &objectJson, bool &arrayFinished);
void sendHttpRequest(WiFiClientSecure &client, const char *host, const String &path, const char *referer);
bool waitForHttpResponse(WiFiClientSecure &client);
void skipHttpHeaders(WiFiClientSecure &client);
bool handleJsonStream(WiFiClientSecure &client, bool startupRecovery, const char *sourceLabel);
bool hasSavedWifiCredentials();
void configureWifiMode(wifi_mode_t mode);
void startStationMode();
bool connectToSavedWifi(unsigned long timeoutMs, bool logAttempt);
void updateStatusLeds();
bool isWifiConnected();
bool wifiCanStartNewAttempt();
void handleWiFiEvent(WiFiEvent_t event);
int classifyEventCategory(JsonObjectConst object);
bool pollFeed(const char *label,
              const char *path,
              bool startupRecovery,
              uint8_t &consecutiveFailures,
              bool &failureLogged);

WebUiContext webUiContext = {
    &server,
    &config,
    &runtimeConfigPortalActive,
    saveRuntimeConfig,
    retryWifiNow,
    restartRuntimeConfigPortal,
    logLine,
    normalizeAreaValue,
};

void logLine(const String &line) {
  if (OREF_SERIAL_LOGGING) {
    Serial.println(line);
  }
}

void loadRuntimeConfig() {
  prefs.begin("oref-relay", true);
  config.area = normalizeAreaValue(prefs.getString("area", OREF_DEFAULT_AREA));
  // If stored value looks like HTML entities, it's corrupted — reset to default.
  if (config.area.isEmpty()) config.area = OREF_DEFAULT_AREA;
  config.relayPin = OREF_DEFAULT_RELAY_PIN;
  config.partial = prefs.getBool("partial", OREF_DEFAULT_ALLOW_PARTIAL_AREA_MATCH);
  config.preAlert = prefs.getBool("prealert", OREF_DEFAULT_TRIGGER_ON_PRE_ALERT);
  config.wifiSsid = prefs.getString("wifi_ssid", "");
  config.wifiPassword = prefs.getString("wifi_password", "");
  prefs.end();
}

void saveRuntimeConfig() {
  prefs.begin("oref-relay", false);
  prefs.putString("area", config.area);
  prefs.putBool("partial", config.partial);
  prefs.putBool("prealert", config.preAlert);
  prefs.putString("wifi_ssid", config.wifiSsid);
  prefs.putString("wifi_password", config.wifiPassword);
  prefs.remove("relay_pin");
  prefs.end();
}

String urlDecode(const String &input) {
  String output;
  output.reserve(input.length());
  for (size_t i = 0; i < input.length(); ++i) {
    if (input.charAt(i) == '%' && i + 2 < input.length()) {
      char buf[3] = { input.charAt(i + 1), input.charAt(i + 2), 0 };
      output += static_cast<char>(strtol(buf, nullptr, 16));
      i += 2;
    } else if (input.charAt(i) == '+') {
      output += ' ';
    } else {
      output += input.charAt(i);
    }
  }
  return output;
}

String trimCopy(String value) {
  value.trim();
  return value;
}

String normalizeAreaValue(const String &value) {
  String area = trimCopy(decodeHtmlEntities(urlDecode(value)));
  return area.isEmpty() ? String(OREF_DEFAULT_AREA) : area;
}
String decodeHtmlEntities(const String &value) {
  String decoded;
  decoded.reserve(value.length());

  for (size_t i = 0; i < value.length(); ++i) {
    if (value.charAt(i) == '&' && i + 3 < value.length() && value.charAt(i + 1) == '#') {
      size_t cursor = i + 2;
      bool hex = false;
      if (cursor < value.length() && (value.charAt(cursor) == 'x' || value.charAt(cursor) == 'X')) {
        hex = true;
        ++cursor;
      }

      size_t digitsStart = cursor;
      while (cursor < value.length() && isxdigit(static_cast<unsigned char>(value.charAt(cursor)))) {
        ++cursor;
      }

      if (cursor > digitsStart && cursor < value.length() && value.charAt(cursor) == ';') {
        const String digits = value.substring(digitsStart, cursor);
        const long codepoint = strtol(digits.c_str(), nullptr, hex ? 16 : 10);

        if (codepoint > 0 && codepoint <= 0x10FFFF) {
          if (codepoint <= 0x7F) {
            decoded += static_cast<char>(codepoint);
          } else if (codepoint <= 0x7FF) {
            decoded += static_cast<char>(0xC0 | ((codepoint >> 6) & 0x1F));
            decoded += static_cast<char>(0x80 | (codepoint & 0x3F));
          } else if (codepoint <= 0xFFFF) {
            decoded += static_cast<char>(0xE0 | ((codepoint >> 12) & 0x0F));
            decoded += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
            decoded += static_cast<char>(0x80 | (codepoint & 0x3F));
          } else {
            decoded += static_cast<char>(0xF0 | ((codepoint >> 18) & 0x07));
            decoded += static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
            decoded += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
            decoded += static_cast<char>(0x80 | (codepoint & 0x3F));
          }
          i = cursor;
          continue;
        }
      }
    }

    decoded += value.charAt(i);
  }

  return decoded;
}

String readHttpLine(WiFiClientSecure &client) {
  String line = client.readStringUntil('\n');
  line.replace("\r", "");
  return line;
}

String readStringField(JsonObjectConst object, const char *key) {
  return object[key].is<const char *>() ? object[key].as<String>() : "";
}

String readTitle(JsonObjectConst object) {
  const String title = readStringField(object, "title");
  if (!title.isEmpty()) {
    return title;
  }

  const String categoryDesc = readStringField(object, "category_desc");
  if (!categoryDesc.isEmpty()) {
    return categoryDesc;
  }

  return readStringField(object, "desc");
}

int mapCategoryName(const String &category) {
  if (category == "missilealert") return 1;
  if (category == "uav") return 2;
  if (category == "update") return 13;
  if (category == "flash") return 14;
  return -1;
}

String readAlertDate(JsonObjectConst object) {
  if (object["alertDate"].is<const char *>()) {
    return object["alertDate"].as<String>();
  }
  if (object["alertDate"].is<long>()) {
    return String(object["alertDate"].as<long>());
  }

  const String date = readStringField(object, "date");
  const String time = readStringField(object, "time");
  return !date.isEmpty() ? (time.isEmpty() ? date : date + " " + time) : time;
}

long readIntField(JsonObjectConst object, const char *key, long fallback = -1) {
  if (object[key].is<long>()) {
    return object[key].as<long>();
  }
  if (object[key].is<int>()) {
    return object[key].as<int>();
  }
  if (object[key].is<const char *>()) {
    const String value = object[key].as<String>();
    if (!value.isEmpty()) {
      return value.toInt();
    }
  }
  return fallback;
}

long readRid(JsonObjectConst object) {
  return readIntField(object, "rid");
}

int readUpdateType(JsonObjectConst object) {
  return static_cast<int>(readIntField(object, "updateType"));
}

int readCategory(JsonObjectConst object) {
  const long category = readIntField(object, "category");
  if (category >= 0) {
    return static_cast<int>(category);
  }
  const long legacyCategory = readIntField(object, "cat");
  if (legacyCategory >= 0) {
    return static_cast<int>(legacyCategory);
  }
  return mapCategoryName(readStringField(object, "cat"));
}

bool readNextJsonObject(WiFiClientSecure &client, String &objectJson, bool &arrayFinished) {
  objectJson = "";
  arrayFinished = false;
  bool started = false;
  bool inString = false;
  bool escaped = false;
  int depth = 0;
  unsigned long lastDataMs = millis();

  while ((client.connected() || client.available()) && millis() - lastDataMs < 30000UL) {
    if (!client.available()) {
      server.handleClient();
      delay(1);
      continue;
    }

    const char c = static_cast<char>(client.read());
    lastDataMs = millis();

    if (!started) {
      if (c == '\xEF' || c == '\xBB' || c == '\xBF' ||
          c == '[' || c == ',' || c == '\r' || c == '\n' || c == '\t' || c == ' ') {
        continue;
      }
      if (c == ']') {
        arrayFinished = true;
        return false;
      }
      if (c != '{') {
        continue;
      }

      started = true;
      depth = 1;
      objectJson.reserve(512);
      objectJson += c;
      continue;
    }

    objectJson += c;

    if (inString) {
      if (escaped) {
        escaped = false;
      } else if (c == '\\') {
        escaped = true;
      } else if (c == '"') {
        inString = false;
      }
      continue;
    }

    if (c == '"') {
      inString = true;
    } else if (c == '{') {
      ++depth;
    } else if (c == '}') {
      --depth;
      if (depth == 0) {
        return true;
      }
    }
  }

  return false;
}

void sendHttpRequest(WiFiClientSecure &client, const char *host, const String &path, const char *referer) {
  client.print(
      String("GET ") + path + " HTTP/1.0\r\n" +
      "Host: " + host + "\r\n" +
      "Referer: " + referer + "\r\n" +
      "X-Requested-With: XMLHttpRequest\r\n" +
      "Accept: application/json, text/plain, */*\r\n" +
      "Accept-Encoding: identity\r\n" +
      "Cache-Control: no-cache\r\n" +
      "Pragma: no-cache\r\n" +
      "User-Agent: " + String(OREF_USER_AGENT) + "\r\n" +
      "Connection: close\r\n\r\n");
}

bool waitForHttpResponse(WiFiClientSecure &client) {
  const unsigned long responseStartMs = millis();
  while (!client.available() && client.connected() && millis() - responseStartMs < OREF_HTTP_TIMEOUT_MS) {
    server.handleClient();
    delay(5);
  }

  const String statusLine = readHttpLine(client);
  if (statusLine.isEmpty()) {
    return false;
  }
  if (!statusLine.startsWith("HTTP/1.1 200") && !statusLine.startsWith("HTTP/1.0 200")) {
    logLine(String("[history-http] ") + statusLine);
    return false;
  }
  return true;
}

void skipHttpHeaders(WiFiClientSecure &client) {
  while (client.connected()) {
    const String headerLine = readHttpLine(client);
    if (headerLine.isEmpty()) break;
  }
}

bool pollEndpoint(const char *host, const char *path, const char *referer, bool startupRecovery, const char *sourceLabel) {
  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(OREF_HTTP_TIMEOUT_MS / 1000);

  if (!client.connect(host, 443)) {
    return false;
  }

  String requestPath = path;
  requestPath += (requestPath.indexOf('?') < 0) ? "?_=" : "&_=";
  requestPath += String(millis());
  sendHttpRequest(client, host, requestPath, referer);

  if (!waitForHttpResponse(client)) {
    client.stop();
    return false;
  }

  skipHttpHeaders(client);
  handleJsonStream(client, startupRecovery, sourceLabel);
  client.stop();
  return true;
}

bool areaMatches(const String &candidateRaw) {
  if (config.area == "*" || config.area.equalsIgnoreCase("all")) {
    return true;
  }

  const String candidate = trimCopy(candidateRaw);
  if (candidate.isEmpty()) {
    return false;
  }

  if (config.partial) {
    return candidate.indexOf(config.area) >= 0 || config.area.indexOf(candidate) >= 0;
  }

  return candidate.equals(config.area);
}

bool areaListMatches(JsonVariantConst dataField) {
  if (dataField.is<JsonArrayConst>()) {
    for (JsonVariantConst item : dataField.as<JsonArrayConst>()) {
      if (areaMatches(item.as<String>())) {
        return true;
      }
    }
    return false;
  }

  if (dataField.is<const char *>()) {
    String data = dataField.as<String>();
    int start = 0;
    while (start >= 0) {
      const int separator = data.indexOf(',', start);
      const String token = separator >= 0 ? data.substring(start, separator) : data.substring(start);
      if (areaMatches(token)) {
        return true;
      }
      start = separator >= 0 ? separator + 1 : -1;
    }
  }

  return false;
}

String categoryLabel(int category) {
  switch (category) {
    case 2:
      return "uav_alert";
    case 13:
      return "end";
    case 14:
      return "pre_alert";
    default:
      return "alert";
  }
}

void setRelay(bool enabled) {
  digitalWrite(config.relayPin, enabled ? HIGH : LOW);
}

void initRelayOutput() {
  pinMode(config.relayPin, OUTPUT);
  digitalWrite(config.relayPin, LOW);
}

void initStatusLeds() {
  pinMode(OREF_RED_LED_PIN, OUTPUT);
  pinMode(OREF_GREEN_LED_PIN, OUTPUT);
  updateStatusLeds();
}

void setStatusLeds(bool wifiConnected) {
  digitalWrite(OREF_RED_LED_PIN, wifiConnected ? LOW : HIGH);
  digitalWrite(OREF_GREEN_LED_PIN, wifiConnected ? HIGH : LOW);
}

void updateStatusLeds() {
  setStatusLeds(isWifiConnected());
}

bool isWifiConnected() {
  return wifiStationHasIp && WiFi.status() == WL_CONNECTED;
}

bool wifiCanStartNewAttempt() {
  const wl_status_t status = WiFi.status();
  return status == WL_DISCONNECTED ||
         status == WL_NO_SSID_AVAIL ||
         status == WL_CONNECT_FAILED ||
         status == WL_CONNECTION_LOST ||
         status == WL_IDLE_STATUS;
}

void handleWiFiEvent(WiFiEvent_t event) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      wifiStationHasIp = true;
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
    case ARDUINO_EVENT_WIFI_STA_LOST_IP:
      wifiStationHasIp = false;
      break;
    default:
      break;
  }
  updateStatusLeds();
}

bool shouldRelayBeOn(int category) {
  if (category == 13) {
    return false;
  }

  if (category == 14) {
    return config.preAlert;
  }

  return category == 1 || category == 2;
}

String makeSignature(JsonObjectConst object) {
  String signature = readStringField(object, "id");
  if (signature.isEmpty()) {
    signature = String(readRid(object));
  }
  if (signature.isEmpty() || signature == "-1") {
    signature = readAlertDate(object);
  }
  if (signature.isEmpty()) {
    signature = readTitle(object);
  }
  signature += "|" + String(classifyEventCategory(object)) + "|";

  if (object["data"].is<JsonArrayConst>()) {
    for (JsonVariantConst item : object["data"].as<JsonArrayConst>()) {
      signature += item.as<String>() + ",";
    }
    return signature;
  }

  return signature + readStringField(object, "data");
}

String makeLogicalSignature(JsonObjectConst object) {
  String signature = String(classifyEventCategory(object)) + "|" + readTitle(object) + "|";

  if (object["data"].is<JsonArrayConst>()) {
    for (JsonVariantConst item : object["data"].as<JsonArrayConst>()) {
      signature += trimCopy(item.as<String>()) + ",";
    }
    return signature;
  }

  return signature + trimCopy(readStringField(object, "data"));
}

int classifyEventCategory(JsonObjectConst object) {
  const int updateType = readUpdateType(object);
  if (updateType == 1 || updateType == 9) {
    return 14;
  }
  if (updateType == 4 || updateType == 13 || updateType == 21) {
    return 13;
  }

  return readCategory(object);
}
bool applyEvent(JsonObjectConst object, String &lastSignature, bool startupRecovery) {
  // Ignore events that are not for the configured area.
  if (!areaListMatches(object["data"])) {
    return false;
  }

  const int category = classifyEventCategory(object);
  if (category < 0) {
    logLine("[poll] skipped matching area event without category");
    return false;
  }

  const String signature = makeSignature(object);
  const String logicalSignature = makeLogicalSignature(object);
  // Skip duplicates during normal polling so the relay does not chatter.
  if (!startupRecovery && signature == lastSignature) {
    return false;
  }
  if (!startupRecovery && logicalSignature == lastAppliedLogicalSignature) {
    lastSignature = signature;
    return false;
  }

  if (startupRecovery && category == 13) {
    lastSignature = signature;
    lastAppliedLogicalSignature = logicalSignature;
    return false;
  }

  lastSignature = signature;
  lastAppliedLogicalSignature = logicalSignature;
  const bool relayEnabled = shouldRelayBeOn(category);
  setRelay(relayEnabled);

  const String alertDate = readAlertDate(object);
  logLine(String("[alert] ") + categoryLabel(category) +
          " state=" + (relayEnabled ? "ON" : "OFF") +
          (alertDate.isEmpty() ? "" : String(" date=") + alertDate));
  return true;
}

String normalizeDate(String value) {
  value.replace('T', ' ');
  const int plusIndex = value.indexOf('+');
  if (plusIndex > 0) {
    value = value.substring(0, plusIndex);
  }
  value.trim();
  return value;
}

bool newerThan(const String &candidate, const String &currentBest) {
  if (currentBest.isEmpty()) {
    return true;
  }
  return normalizeDate(candidate) > normalizeDate(currentBest);
}
bool handleJsonStream(WiFiClientSecure &client, bool startupRecovery, const char *sourceLabel) {
  const bool isRealtime = String(sourceLabel) == "realtime";
  String &lastSignature = isRealtime ? lastRealtimeSignature : lastHistorySignature;
  String &lastInfoSignature = isRealtime ? lastRealtimeInfoSignature : lastHistoryInfoSignature;
  String &noMatchSignature = isRealtime ? lastRealtimeNoMatchSignature : lastHistoryNoMatchSignature;
  long bestRid = -1;
  String bestDate;
  long newestRid = -1;
  String newestDate;
  String newestInfoSignature;
  String objectJson;
  String bestObjectJson;
  String firstParseError;
  int matchingAreaObjects = 0;
  int parseErrors = 0;
  bool arrayFinished = false;
  DynamicJsonDocument doc(1024);

  while (readNextJsonObject(client, objectJson, arrayFinished)) {
    doc.clear();
    const DeserializationError error = deserializeJson(doc, objectJson);
    if (error || !doc.is<JsonObject>()) {
      ++parseErrors;
      if (firstParseError.isEmpty()) {
        firstParseError = error ? error.c_str() : "not-object";
      }
      continue;
    }

    JsonObjectConst object = doc.as<JsonObjectConst>();
    const int category = classifyEventCategory(object);
    const String title = readTitle(object);
    const String dateValue = readAlertDate(object);
    const long rid = readRid(object);

    const String infoSignature = String(category) + "|" + title + "|" + dateValue;
    if (rid > newestRid || (rid < 0 && newerThan(dateValue, newestDate))) {
      newestRid = rid;
      newestDate = dateValue;
      newestInfoSignature = infoSignature;
    }

    if (!areaListMatches(object["data"])) {
      continue;
    }

    ++matchingAreaObjects;
    if (rid > bestRid || (rid < 0 && newerThan(dateValue, bestDate))) {
      bestRid = rid;
      bestDate = dateValue;
      bestObjectJson = objectJson;
    }
  }

  if (bestObjectJson.isEmpty()) {
    if (parseErrors > 0) {
      logLine(String("[") + sourceLabel + "] no usable record parsed, errors=" + parseErrors +
              (firstParseError.isEmpty() ? "" : String(" first_error=") + firstParseError));
    }
    return false;
  }

  doc.clear();
  const DeserializationError bestError = deserializeJson(doc, bestObjectJson);
  if (bestError || !doc.is<JsonObject>()) {
    logLine(String("[poll] ") + sourceLabel + " best-record parse failed: " + bestError.c_str());
    return false;
  }

  JsonObjectConst bestMatch = doc.as<JsonObjectConst>();
  const int category = classifyEventCategory(bestMatch);
  const String title = readTitle(bestMatch);
  const String alertDate = readAlertDate(bestMatch);
  const String infoSignature = String(category) + "|" + title + "|" + alertDate;
  if (infoSignature != lastInfoSignature) {
    lastInfoSignature = infoSignature;
    noMatchSignature = "";
  }

  return applyEvent(bestMatch, lastSignature, startupRecovery);
}

void configureWifiMode(wifi_mode_t mode) {
  WiFi.mode(mode);
  WiFi.setHostname(OREF_HOSTNAME);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);
  esp_wifi_set_max_tx_power(OREF_WIFI_TX_POWER_QUARTER_DBM);
}

void ensureWifiConnected() {
  if (isWifiConnected()) {
    if (runtimeConfigPortalActive) {
      stopRuntimeConfigPortal();
      startWebUi();
    }
    wifiPortalOpenedForCurrentDisconnect = false;
    updateStatusLeds();
    return;
  }

  setStatusLeds(false);

  if (runtimeConfigPortalActive) {
    return;
  }

  if (connectToSavedWifi(OREF_WIFI_CONNECT_TIMEOUT_MS, true)) {
    wifiPortalOpenedForCurrentDisconnect = false;
    logLine(String("[wifi] connected, ip=") + WiFi.localIP().toString());
  } else {
    logLine("[wifi] connect timeout, opening config portal");
    startRuntimeConfigPortal();
  }
  updateStatusLeds();
}

bool hasSavedWifiCredentials() {
  return !trimCopy(config.wifiSsid).isEmpty();
}

void startStationMode() {
  configureWifiMode(WIFI_STA);
}

bool connectToSavedWifi(unsigned long timeoutMs, bool logAttempt) {
  if (!hasSavedWifiCredentials()) {
    if (logAttempt) {
      logLine("[wifi] no saved credentials");
    }
    return false;
  }

  startStationMode();
  if (logAttempt) {
    logLine(String("[wifi] connecting to ") + config.wifiSsid);
  }
  WiFi.disconnect(false, false);
  delay(50);
  WiFi.begin(config.wifiSsid.c_str(), config.wifiPassword.c_str());

  if (timeoutMs == 0) {
    return isWifiConnected();
  }

  const unsigned long startMs = millis();
  while (!isWifiConnected() && millis() - startMs < timeoutMs) {
    delay(250);
  }

  return isWifiConnected();
}

void startWebUi() {
  webUiBegin(webUiContext);
}

void startRuntimeConfigPortal() {
  if (runtimeConfigPortalActive) {
    return;
  }

  configureWifiMode(hasSavedWifiCredentials() ? WIFI_AP_STA : WIFI_AP);
  WiFi.softAP(
      OREF_CONFIG_AP_NAME,
      OREF_CONFIG_AP_PASSWORD[0] == '\0' ? nullptr : OREF_CONFIG_AP_PASSWORD);
  if (hasSavedWifiCredentials()) {
    WiFi.begin(config.wifiSsid.c_str(), config.wifiPassword.c_str());
  }
  runtimeConfigPortalActive = true;
  lastWifiRetryWhilePortalMs = 0;
  startWebUi();
  logLine(String("[wifi] portal ap=") + WiFi.softAPIP().toString());
}

void stopRuntimeConfigPortal() {
  if (!runtimeConfigPortalActive) {
    return;
  }

  WiFi.softAPdisconnect(true);
  startStationMode();
  runtimeConfigPortalActive = false;
}

void restartRuntimeConfigPortal() {
  stopRuntimeConfigPortal();
  startRuntimeConfigPortal();
}

void retryWifiNow() {
  lastWifiRetryWhilePortalMs = 0;
  wifiStationHasIp = false;
  if (hasSavedWifiCredentials()) {
    WiFi.disconnect(false, false);
    delay(50);
    if (runtimeConfigPortalActive) {
      configureWifiMode(WIFI_AP_STA);
    } else {
      startStationMode();
    }
    WiFi.begin(config.wifiSsid.c_str(), config.wifiPassword.c_str());
  }
}

bool pollFeed(const char *label,
              const char *path,
              bool startupRecovery,
              uint8_t &consecutiveFailures,
              bool &failureLogged) {
  if (!pollEndpoint(kOrefHost, path, kOrefReferer, startupRecovery, label)) {
    ++consecutiveFailures;
    if (!failureLogged && consecutiveFailures >= 3) {
      logLine(String("[poll] ") + label + " fetch failed for https://" + kOrefHost + path);
      failureLogged = true;
    }
    return false;
  }

  consecutiveFailures = 0;
  if (failureLogged) {
    logLine(String("[poll] ") + label + " fetch recovered");
    failureLogged = false;
  }
  return true;
}
}  // namespace

void setup() {
  if (OREF_SERIAL_LOGGING) {
    Serial.begin(115200);
    delay(200);
  }

  loadRuntimeConfig();

  // Active-high relay: LOW means idle at boot.
  initRelayOutput();
  initStatusLeds();

  WiFi.onEvent(handleWiFiEvent);

  logLine(String("[config] area=") + config.area +
          " partial=" + (config.partial ? "1" : "0") +
          " prealert=" + (config.preAlert ? "1" : "0"));

  ensureWifiConnected();

  // Try to recover the last known state immediately after Wi-Fi comes up.
  if (isWifiConnected()) {
    startWebUi();
    startupRecoveryPending = true;
  }
}

void loop() {
  // Main loop: keep Wi-Fi alive and poll the alert endpoints.
  ensureWifiConnected();

  const bool wifiConnected = isWifiConnected();
  if (!wifiConnected && !runtimeConfigPortalActive) {
    updateStatusLeds();
    delay(250);
    return;
  }

  updateStatusLeds();
  server.handleClient();

  if (!wifiConnected) {
    delay(25);
    return;
  }

  unsigned long now = millis();

  if (startupRecoveryPending) {
    startupRecoveryPending = false;
    lastRealtimePollMs = now;
    lastHistoryPollMs = now;
    pollFeed("realtime", kRealtimePath, true, consecutiveRealtimeFetchFailures, realtimeFetchFailureLogged);
    server.handleClient();
    pollFeed("history", kHistoryPath, true, consecutiveHistoryFetchFailures, historyFetchFailureLogged);
    now = millis();
  }

  if (now - lastRealtimePollMs >= OREF_REALTIME_POLL_MS) {
    lastRealtimePollMs = now;
    pollFeed("realtime", kRealtimePath, false, consecutiveRealtimeFetchFailures, realtimeFetchFailureLogged);
  }

  if (now - lastHistoryPollMs >= OREF_HISTORY_POLL_MS) {
    lastHistoryPollMs = now;
    pollFeed("history", kHistoryPath, false, consecutiveHistoryFetchFailures, historyFetchFailureLogged);
  }

  delay(25);
}

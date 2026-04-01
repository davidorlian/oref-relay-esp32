#include "web_ui.h"

#include <WiFi.h>

namespace {
WebUiContext *g_ctx = nullptr;
bool g_configured = false;

String htmlEscape(const String &value) {
  String escaped = value;
  escaped.replace("&", "&amp;");
  escaped.replace("<", "&lt;");
  escaped.replace(">", "&gt;");
  escaped.replace("\"", "&quot;");
  return escaped;
}

void logSettingsSaveChanges(const String &previousArea, bool previousPartial, bool previousPreAlert) {
  RuntimeConfig &config = *g_ctx->config;
  String changes;
  if (previousArea != config.area) {
    changes += String(" area='") + previousArea + "'->'" + config.area + "'";
  }
  if (previousPartial != config.partial) {
    changes += String(" partial=") + String(previousPartial ? 1 : 0) + "->" + String(config.partial ? 1 : 0);
  }
  if (previousPreAlert != config.preAlert) {
    changes += String(" prealert=") + String(previousPreAlert ? 1 : 0) + "->" + String(config.preAlert ? 1 : 0);
  }
  g_ctx->logLine(changes.isEmpty() ? "[web] settings saved (no changes)" : String("[web] settings saved") + changes);
}

String buildSettingsPage(bool wifiOnly) {
  RuntimeConfig &config = *g_ctx->config;
  const bool runtimePortalActive = *g_ctx->runtimeConfigPortalActive;
  String page;
  page.reserve(7800);
  page += F(
      "<!doctype html><html lang='he' dir='rtl'><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
      "<title>הגדרות Oref Relay</title><style>"
      ":root{--card:#fffdf9;--line:#d9cfc1;--text:#1f1a17;--muted:#6f655d;--primary:#b55233;--primary-dark:#8f3e24;--input-bg:#f7f2ec;--input-border:#c9beb0;}"
      "*{box-sizing:border-box;}"
      "body{margin:0;padding:28px 16px 40px;background:linear-gradient(160deg,#ece3d8 0%,#f8f4ed 60%,#eee9e0 100%);color:var(--text);font-family:'Segoe UI',Tahoma,sans-serif;min-height:100vh;}"
      ".card{max-width:520px;margin:0 auto;background:var(--card);border:1px solid rgba(0,0,0,.06);border-radius:20px;padding:28px 22px 24px;box-shadow:0 16px 48px rgba(65,46,33,.12),0 2px 8px rgba(65,46,33,.06);}"
      ".card-header{margin-bottom:20px;}"
      "h2{margin:0 0 10px;font-size:24px;font-weight:800;letter-spacing:-.4px;color:var(--text);}"
      ".meta-row{display:flex;align-items:center;justify-content:space-between;gap:10px;margin-bottom:4px;}"
      ".meta{color:var(--muted);font-size:13px;line-height:1.6;}"
      ".meta strong{color:var(--primary-dark);font-weight:700;}"
      ".wifi-inline-btn,.wifi-link{display:inline-flex;align-items:center;justify-content:center;gap:5px;padding:8px 12px;border-radius:999px;border:1.5px solid rgba(181,82,51,.3);background:#efe4d8;color:var(--primary-dark);font-size:12px;font-weight:700;text-decoration:none;white-space:nowrap;font-family:inherit;cursor:pointer;}"
      ".wifi-inline-btn:hover,.wifi-link:hover{background:#e4d5c5;border-color:var(--primary);}"
      ".grid{display:grid;gap:12px;}"
      ".field-card,.switch-row{padding:14px 16px;border:1px solid var(--line);border-radius:14px;background:#fff;}"
      ".field-card{transition:border-color .15s,box-shadow .15s;}"
      ".field-card:focus-within{border-color:var(--primary);box-shadow:0 0 0 3px rgba(181,82,51,.10);}"
      ".field-label{display:block;font-weight:700;font-size:11px;color:var(--muted);text-transform:uppercase;letter-spacing:.6px;margin-bottom:4px;}"
      "input[type='text'],input[type='password']{display:block;width:100%;padding:10px 12px;border:1.5px solid var(--input-border);border-radius:10px;font-size:15px;font-family:inherit;background:var(--input-bg);color:var(--text);outline:none;transition:border-color .15s,box-shadow .15s;margin-top:6px;}"
      "input[type='text']:focus,input[type='password']:focus{border-color:var(--primary);box-shadow:0 0 0 3px rgba(181,82,51,.12);}"
      "input::placeholder{color:#b8afa6;font-size:14px;}"
      ".hint{font-size:12px;color:var(--muted);margin-top:7px;line-height:1.5;}"
      ".section{padding-top:16px;margin-top:16px;border-top:1px solid rgba(0,0,0,.07);display:grid;gap:10px;}"
      "button{width:100%;border-radius:12px;font-size:15px;font-family:inherit;padding:13px 14px;border:none;font-weight:700;cursor:pointer;transition:background .15s ease,transform .1s ease,box-shadow .15s;}"
      "button:active{transform:scale(.98);}"
      "button.primary{background:var(--primary);color:#fff;box-shadow:0 4px 16px rgba(181,82,51,.30);}"
      "button.primary:hover{background:var(--primary-dark);box-shadow:0 6px 20px rgba(181,82,51,.38);}"
      "button.secondary{background:#efe4d8;color:var(--primary-dark);}"
      ".switch-row{display:flex;align-items:center;justify-content:space-between;gap:16px;}"
      ".switch-copy{flex:1;}"
      ".switch-copy strong{display:block;font-size:15px;font-weight:700;color:var(--text);}"
      ".switch-copy span{display:block;margin-top:3px;font-size:12px;color:var(--muted);line-height:1.5;}"
      ".switch-state{display:inline-block;margin-top:8px;padding:3px 10px;border-radius:999px;background:#efe4d8;color:var(--primary-dark);font-size:11px;font-weight:700;}"
      ".switch{position:relative;display:inline-block;width:54px;height:30px;flex:0 0 auto;}"
      ".switch input{opacity:0;width:0;height:0;position:absolute;}"
      ".slider{position:absolute;cursor:pointer;inset:0;background:#c9beb0;border-radius:999px;transition:.2s;}"
      ".slider:before{position:absolute;content:'';height:24px;width:24px;right:3px;top:3px;background:#fff;border-radius:50%;transition:.2s;box-shadow:0 1px 4px rgba(0,0,0,.2);}"
      ".switch input:checked + .slider{background:var(--primary);}"
      ".switch input:checked + .slider:before{transform:translateX(-24px);}"
      ".wifi-modal-overlay{display:none;position:fixed;inset:0;background:rgba(30,20,14,.45);backdrop-filter:blur(4px);z-index:200;align-items:flex-end;justify-content:center;}"
      ".wifi-modal-overlay.open{display:flex;}"
      ".wifi-modal{background:var(--card);border-radius:20px 20px 0 0;padding:24px 22px 40px;width:100%;max-width:520px;box-shadow:0 -8px 40px rgba(65,46,33,.18);animation:slideUp .22s ease;}"
      "@keyframes slideUp{from{transform:translateY(60px);opacity:0;}to{transform:translateY(0);opacity:1;}}"
      ".wifi-modal-header{display:flex;align-items:center;justify-content:space-between;margin-bottom:18px;}"
      ".wifi-modal-header h3{margin:0;font-size:18px;font-weight:800;}"
      ".wifi-modal-close{background:#efe4d8;border:none;border-radius:999px;font-size:13px;cursor:pointer;color:var(--primary-dark);padding:6px 12px;font-weight:700;font-family:inherit;width:auto;}"
      "</style></head><body><div class='card'><div class='card-header'><h2>");

  page += wifiOnly ? "הגדרות Wi-Fi" : "הגדרות הממסר";
  page += F("</h2><div class='meta-row'>");
  page += "<span class='meta'>כתובת המכשיר: " + WiFi.localIP().toString();
  if (!wifiOnly) {
    page += "<br>אזור נוכחי: <strong>" + htmlEscape(config.area) + "</strong>";
  }
  page += "</span>";

  if (!wifiOnly) {
    page += F(
        "<button type='button' class='wifi-inline-btn' onclick='showWifiPage()'>"
        "<span style='display:block;line-height:1.2'>הגדרות<br>Wi-Fi</span>"
        "<svg width='13' height='13' viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='2.5' stroke-linecap='round' stroke-linejoin='round'>"
        "<path d='M5 12.55a11 11 0 0 1 14.08 0'/><path d='M1.42 9a16 16 0 0 1 21.16 0'/>"
        "<path d='M8.53 16.11a6 6 0 0 1 6.95 0'/><circle cx='12' cy='20' r='1' fill='currentColor'/>"
        "</svg></button>");
  }
  page += F("</div>");

  if (runtimePortalActive) {
    page += "<p class='meta'>פורטל ההגדרה פעיל: " + WiFi.softAPIP().toString() + "</p>";
  }

  page += F("</div><form id='settings-form' method='post' action='/save'>");
  page += "<input type='hidden' name='_page' value='";
  page += wifiOnly ? "wifi" : "root";
  page += "'>";
  page += F("<div class='grid'>");

  if (wifiOnly) {
    page += "<input type='hidden' name='area' value='" + htmlEscape(config.area) + "'>";
    page += "<input type='hidden' name='partial' value='";
    page += config.partial ? "1" : "0";
    page += "'><input type='hidden' name='prealert' value='";
    page += config.preAlert ? "1" : "0";
    page += "'>";

    page += F("<div class='field-card'><label class='field-label' for='wifi_ssid'>שם רשת Wi-Fi</label>");
    page += "<input id='wifi_ssid' name='wifi_ssid' type='text' value='" + htmlEscape(config.wifiSsid) + "' maxlength='63' autocomplete='username' placeholder='הזן שם רשת'>";
    page += F("<div class='hint'>הרשת שאליה המכשיר צריך להתחבר.</div></div>");
    page += F("<div class='field-card'><label class='field-label' for='wifi_password'>סיסמת Wi-Fi</label>");
    page += F("<input id='wifi_password' name='wifi_password' type='password' value='' maxlength='63' autocomplete='current-password' placeholder='השאר ריק לשמירת הסיסמה הנוכחית'>");
    page += F("<div class='hint'>השאר ריק כדי לשמור את הסיסמה הקיימת.</div></div>");
  } else {
    page += F("<div class='field-card'><label class='field-label' for='area'>אזור התרעה</label>");
    page += "<input id='area' name='area' type='text' value='" + htmlEscape(config.area) + "' maxlength='95' placeholder='לדוגמה: רחובות'>";
    page += F("<div class='hint'>יש להזין את שם האזור כפי שמופיע בהתראות פיקוד העורף.</div></div>");

    page += F("<div class='switch-row'><div class='switch-copy'><strong>התאמה חלקית</strong><span>להתאמה גם כששם האזור מופיע כחלק משם ארוך יותר.</span><div id='partial-state' class='switch-state'>");
    page += config.partial ? "פועל: התאמה חלקית" : "פועל: התאמה מלאה";
    page += F("</div></div><input id='partial_value' type='hidden' name='partial' value='");
    page += config.partial ? "1" : "0";
    page += F("'><label class='switch'><input id='partial' type='checkbox' value='1'");
    page += config.partial ? " checked" : "";
    page += F("><span class='slider'></span></label></div>");

    page += F("<div class='switch-row'><div class='switch-copy'><strong>הפעלת הממסר בהתראה מקדימה</strong><span>כשהאפשרות כבויה, הממסר יופעל רק באזעקה עצמה.</span><div id='prealert-state' class='switch-state'>");
    page += config.preAlert ? "פועל: התראה מקדימה" : "פועל: אזעקה בלבד";
    page += F("</div></div><input id='prealert_value' type='hidden' name='prealert' value='");
    page += config.preAlert ? "1" : "0";
    page += F("'><label class='switch'><input id='prealert' type='checkbox' value='1'");
    page += config.preAlert ? " checked" : "";
    page += F("><span class='slider'></span></label></div>");

    page += F("<div class='wifi-modal-overlay' id='wifi-modal-overlay' onclick='if(event.target===this)hideWifiPage()'><div class='wifi-modal'><div class='wifi-modal-header'><h3>הגדרות Wi-Fi</h3><button type='button' class='wifi-modal-close' onclick='hideWifiPage()'>&#x2715; סגור</button></div><div class='grid'>");
    page += F("<div class='field-card'><label class='field-label' for='wifi_ssid'>שם רשת Wi-Fi</label>");
    page += "<input id='wifi_ssid' name='wifi_ssid' type='text' value='" + htmlEscape(config.wifiSsid) + "' maxlength='63' autocomplete='username' placeholder='הזן שם רשת'>";
    page += F("<div class='hint'>הרשת שאליה המכשיר צריך להתחבר.</div></div>");
    page += F("<div class='field-card'><label class='field-label' for='wifi_password'>סיסמת Wi-Fi</label>");
    page += F("<input id='wifi_password' name='wifi_password' type='password' value='' maxlength='63' autocomplete='current-password' placeholder='השאר ריק לשמירת הסיסמה הנוכחית'>");
    page += F("<div class='hint'>השאר ריק כדי לשמור את הסיסמה הקיימת.</div></div>");
    page += F("<button class='primary' type='submit'>שמירת הגדרות Wi-Fi</button></div></div></div>");
  }

  page += F("</div><div class='section'><button class='primary' type='submit'>שמירת הגדרות</button></div></form>");
  page += F("<div class='section'><form method='post' action='/wifi-reset'><button class='primary' type='submit'>איפוס Wi-Fi</button></form></div>");

  if (wifiOnly) {
    page += F("<div class='section'><a class='wifi-link' href='/'>חזרה לעמוד הראשי</a></div>");
  }

  page += F("<script>"
            "const bindSwitchState=(inputId,valueId,labelId,onText,offText)=>{"
            "const input=document.getElementById(inputId);const value=document.getElementById(valueId);const label=document.getElementById(labelId);"
            "if(!input||!value||!label)return;"
            "const render=()=>{const enabled=input.checked;value.value=enabled?'1':'0';label.textContent=enabled?onText:offText;};"
            "input.addEventListener('change',render);render();};"
            "bindSwitchState('partial','partial_value','partial-state','פועל: התאמה חלקית','פועל: התאמה מלאה');"
            "bindSwitchState('prealert','prealert_value','prealert-state','פועל: התראה מקדימה','פועל: אזעקה בלבד');"
            "function showWifiPage(){const modal=document.getElementById('wifi-modal-overlay');if(!modal)return;modal.classList.add('open');document.body.style.overflow='hidden';}"
            "function hideWifiPage(){const modal=document.getElementById('wifi-modal-overlay');if(!modal)return;modal.classList.remove('open');document.body.style.overflow='';}"
            "</script></body></html>");

  return page;
}

void handleRoot() {
  WebServer &server = *g_ctx->server;
  const String page = buildSettingsPage(false);
  server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "0");
  server.send(200, "text/html; charset=utf-8", page);
}

void handleWifiPage() {
  WebServer &server = *g_ctx->server;
  const String page = buildSettingsPage(true);
  server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "0");
  server.send(200, "text/html; charset=utf-8", page);
}

void handleSave() {
  WebServer &server = *g_ctx->server;
  RuntimeConfig &config = *g_ctx->config;
  const String previousWifiSsid = config.wifiSsid;
  const String previousArea = config.area;
  const bool previousPartial = config.partial;
  const bool previousPreAlert = config.preAlert;

  String nextWifiSsid = server.arg("wifi_ssid");
  const String nextWifiPassword = server.arg("wifi_password");
  nextWifiSsid.trim();
  if (!nextWifiSsid.isEmpty()) {
    config.wifiSsid = nextWifiSsid;
    if (!nextWifiPassword.isEmpty()) {
      config.wifiPassword = nextWifiPassword;
    }
  }

  config.area = g_ctx->normalizeAreaValue(server.arg("area"));
  config.partial = server.arg("partial") == "1";
  config.preAlert = server.arg("prealert") == "1";
  g_ctx->saveRuntimeConfig();

  if (!config.wifiSsid.isEmpty() && (config.wifiSsid != previousWifiSsid || !nextWifiPassword.isEmpty())) {
    if (*g_ctx->runtimeConfigPortalActive) {
      g_ctx->retryWifiNow();
    }
  }

  const bool wifiPage = server.arg("_page") == "wifi";
  server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
  server.sendHeader("Location", wifiPage ? "/wifi" : "/", true);
  server.send(303, "text/plain; charset=utf-8", "נשמר");
  logSettingsSaveChanges(previousArea, previousPartial, previousPreAlert);
}

void handleWifiReset() {
  WebServer &server = *g_ctx->server;
  RuntimeConfig &config = *g_ctx->config;
  config.wifiSsid = "";
  config.wifiPassword = "";
  g_ctx->saveRuntimeConfig();
  WiFi.disconnect(true, false);
  g_ctx->restartRuntimeConfigPortal();
  server.send(200, "text/plain; charset=utf-8", "הגדרות ה-Wi-Fi אופסו. פורטל ההגדרה נשאר פעיל כדי להגדיר רשת חדשה.");
  g_ctx->logLine("[web] Wi-Fi settings cleared");
}
}  // namespace

void webUiBegin(WebUiContext &context) {
  g_ctx = &context;
  if (!g_configured) {
    context.server->on("/", HTTP_GET, handleRoot);
    context.server->on("/wifi", HTTP_GET, handleWifiPage);
    context.server->on("/save", HTTP_POST, handleSave);
    context.server->on("/wifi-reset", HTTP_POST, handleWifiReset);
    g_configured = true;
  }
  context.server->begin();
  const IPAddress ip = WiFi.localIP() != IPAddress((uint32_t)0) ? WiFi.localIP() : WiFi.softAPIP();
  context.logLine(String("[web] ready http://") + ip.toString());
}

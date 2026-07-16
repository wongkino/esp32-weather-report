#include "wifi_portal.h"
#include "wifi_config.h"
#include "districts.h"
#include "touch_cyd.h"
#include "portal_html.h"

#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <ArduinoJson.h>

static Preferences wifiPrefs;
static WebServer server(80);
static DNSServer dnsServer;
static bool portalSuccess = false;
static bool settingsServerRunning = false;
static WifiDistrictChangedFn districtChangedCb = nullptr;
static WifiTouchCalFn touchCalStartCb = nullptr;
static WifiTouchCalFn touchCalResetCb = nullptr;
static WifiTouchCalFn touchCalDoneCb = nullptr;

static String scanJson = "[]";
static bool scanInProgress = false;
static uint32_t lastScanMs = 0;

static const char *AP_SSID = "esp32-weather";
static const char *AP_PASS = "";
static const IPAddress AP_IP(192, 168, 4, 1);

static void sendJson(int code, const char *body) {
  server.send(code, "application/json", body);
}

static void sendJsonOk() {
  sendJson(200, "{\"ok\":true}");
}

static void sendOkFalse(int code, const char *error) {
  String body = "{\"ok\":false,\"error\":\"";
  body += error;
  body += "\"}";
  sendJson(code, body.c_str());
}

int wifiLoadDistrictIndex() {
  wifiPrefs.begin("weather", true);
  const int idx = wifiPrefs.getInt("idx", 0);
  wifiPrefs.end();
  return clampDistrictIndex(idx);
}

void wifiSaveDistrictIndex(int index) {
  index = clampDistrictIndex(index);
  wifiPrefs.begin("weather", false);
  wifiPrefs.putInt("idx", index);
  wifiPrefs.end();
  Serial.printf("[District] saved idx=%d %s\n", index, WEATHER_DISTRICTS[index].label);
}

void wifiSetDistrictChangedCallback(WifiDistrictChangedFn cb) {
  districtChangedCb = cb;
}

void wifiSetTouchCalStartCallback(WifiTouchCalFn cb) {
  touchCalStartCb = cb;
}

void wifiSetTouchCalResetCallback(WifiTouchCalFn cb) {
  touchCalResetCb = cb;
}

void wifiSetTouchCalDoneCallback(WifiTouchCalFn cb) {
  touchCalDoneCb = cb;
}

static void loadSavedCredentials(String &ssid, String &pass) {
  wifiPrefs.begin("wifi", true);
  ssid = wifiPrefs.getString("ssid", "");
  pass = wifiPrefs.getString("pass", "");
  wifiPrefs.end();
}

static void saveCredentials(const String &ssid, const String &pass) {
  wifiPrefs.begin("wifi", false);
  wifiPrefs.putString("ssid", ssid);
  wifiPrefs.putString("pass", pass);
  wifiPrefs.end();
}

static void ensureSoftAp() {
  WiFi.softAPConfig(AP_IP, AP_IP, IPAddress(255, 255, 255, 0));
  if (AP_PASS[0] == '\0') {
    WiFi.softAP(AP_SSID);
  } else {
    WiFi.softAP(AP_SSID, AP_PASS);
  }
}

static void rebuildScanJson(int n) {
  JsonDocument doc;
  doc["status"] = "ok";
  JsonArray arr = doc["networks"].to<JsonArray>();

  int order[64];
  const int count = n > 64 ? 64 : n;
  for (int i = 0; i < count; i++) {
    order[i] = i;
  }
  for (int i = 0; i < count; i++) {
    for (int j = i + 1; j < count; j++) {
      if (WiFi.RSSI(order[j]) > WiFi.RSSI(order[i])) {
        const int t = order[i];
        order[i] = order[j];
        order[j] = t;
      }
    }
  }

  for (int k = 0; k < count; k++) {
    const int i = order[k];
    const String ssid = WiFi.SSID(i);
    if (ssid.isEmpty()) {
      continue;
    }
    JsonObject o = arr.add<JsonObject>();
    o["ssid"] = ssid;
    o["rssi"] = WiFi.RSSI(i);
    o["secure"] = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
  }

  scanJson = "";
  serializeJson(doc, scanJson);
  lastScanMs = millis();
  Serial.printf("[Wi-Fi] scan ready: %u networks\n", (unsigned)arr.size());
}

static void startAsyncScan() {
  if (scanInProgress) {
    return;
  }

  WiFi.mode(WIFI_AP_STA);
  ensureSoftAp();
  WiFi.scanDelete();

  int16_t r = WiFi.scanNetworks(true, false);
  if (r == WIFI_SCAN_FAILED) {
    delay(100);
    r = WiFi.scanNetworks(true, false);
  }

  if (r == WIFI_SCAN_RUNNING) {
    scanInProgress = true;
    Serial.println("[Wi-Fi] async scan started");
    return;
  }

  if (r >= 0) {
    rebuildScanJson(r);
    WiFi.scanDelete();
    ensureSoftAp();
    return;
  }

  Serial.printf("[Wi-Fi] async scan start failed: %d\n", r);
  scanJson = "{\"status\":\"error\",\"error\":\"無法開始掃描\"}";
}

static void pollScanComplete() {
  if (!scanInProgress) {
    return;
  }

  const int16_t n = WiFi.scanComplete();
  if (n == WIFI_SCAN_RUNNING) {
    return;
  }

  scanInProgress = false;
  if (n < 0) {
    Serial.printf("[Wi-Fi] scan complete error: %d\n", n);
    scanJson = "{\"status\":\"error\",\"error\":\"掃描失敗，請再試\"}";
    WiFi.scanDelete();
    ensureSoftAp();
    return;
  }

  rebuildScanJson(n);
  WiFi.scanDelete();
  ensureSoftAp();
}

static bool tryBeginSta(const char *ssid, const char *password, uint32_t timeoutMs,
                        bool keepAp) {
  if (ssid == nullptr || ssid[0] == '\0') {
    return false;
  }

  Serial.printf("[Wi-Fi] Connecting to %s (keepAp=%d)\n", ssid, keepAp ? 1 : 0);
  WiFi.disconnect(false, false);
  delay(100);
  WiFi.mode(keepAp ? WIFI_AP_STA : WIFI_STA);
  if (keepAp) {
    ensureSoftAp();
  }
  WiFi.setSleep(false);
  WiFi.begin(ssid, password != nullptr ? password : "");

  const uint32_t startMs = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startMs < timeoutMs) {
    delay(250);
    Serial.print(".");
    yield();
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("[Wi-Fi] IP: %s\n", WiFi.localIP().toString().c_str());
    return true;
  }
  Serial.printf("[Wi-Fi] failed status=%d\n", WiFi.status());
  return false;
}

bool wifiIsConnected() {
  return WiFi.status() == WL_CONNECTED;
}

String wifiPortalUrl() {
  return String("http://") + AP_IP.toString();
}

bool wifiTryConnect() {
  String savedSsid;
  String savedPass;
  loadSavedCredentials(savedSsid, savedPass);
  if (savedSsid.isEmpty()) {
    Serial.println("[Wi-Fi] no saved credentials in NVS");
    return false;
  }
  return tryBeginSta(savedSsid.c_str(), savedPass.c_str(), WIFI_CONNECT_TIMEOUT_MS, false);
}

static bool portalModeAp = true;

static void handleRoot() {
  server.send_P(200, "text/html; charset=utf-8", PORTAL_HTML);
}

static void handleConfig() {
  JsonDocument doc;
  doc["mode"] = portalModeAp ? "ap" : "sta";
  if (!portalModeAp && WiFi.status() == WL_CONNECTED) {
    doc["ip"] = WiFi.localIP().toString();
  }
  doc["district"] = wifiLoadDistrictIndex();
  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

static void handleDistricts() {
  JsonDocument doc;
  doc["current"] = wifiLoadDistrictIndex();
  JsonArray items = doc["items"].to<JsonArray>();
  for (int i = 0; i < WEATHER_DISTRICT_COUNT; i++) {
    JsonObject o = items.add<JsonObject>();
    o["i"] = i;
    o["label"] = WEATHER_DISTRICTS[i].label;
  }
  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

static void handleDistrictPost() {
  if (!server.hasArg("plain")) {
    sendOkFalse(400, "無內容");
    return;
  }

  JsonDocument doc;
  if (deserializeJson(doc, server.arg("plain"))) {
    sendOkFalse(400, "JSON 無效");
    return;
  }

  int idx = doc["district"] | -1;
  if (idx < 0 || idx >= WEATHER_DISTRICT_COUNT) {
    sendOkFalse(400, "地區無效");
    return;
  }

  wifiSaveDistrictIndex(idx);
  if (districtChangedCb) {
    districtChangedCb(idx);
  }

  String resp = "{\"ok\":true,\"label\":\"";
  resp += WEATHER_DISTRICTS[idx].label;
  resp += "\"}";
  sendJson(200, resp.c_str());
}

static void handleScan() {
  if (!portalModeAp) {
    sendJson(400, "{\"status\":\"error\",\"error\":\"已連線模式\"}");
    return;
  }

  const bool refresh = server.hasArg("refresh");
  if (refresh && !scanInProgress) {
    startAsyncScan();
  }

  if (scanInProgress) {
    sendJson(200, "{\"status\":\"scanning\"}");
    return;
  }

  if (lastScanMs == 0 && scanJson == "[]") {
    startAsyncScan();
    sendJson(200, "{\"status\":\"scanning\"}");
    return;
  }

  sendJson(200, scanJson.c_str());
}

static void handleConnect() {
  if (!portalModeAp) {
    sendOkFalse(400, "已連線模式");
    return;
  }
  if (!server.hasArg("plain")) {
    sendOkFalse(400, "無內容");
    return;
  }

  JsonDocument doc;
  if (deserializeJson(doc, server.arg("plain"))) {
    sendOkFalse(400, "JSON 無效");
    return;
  }

  const char *ssid = doc["ssid"] | "";
  const char *pass = doc["pass"] | "";
  int district = clampDistrictIndex(doc["district"] | wifiLoadDistrictIndex());
  if (ssid[0] == '\0') {
    sendOkFalse(400, "SSID 空白");
    return;
  }

  if (scanInProgress) {
    WiFi.scanDelete();
    scanInProgress = false;
  }

  wifiSaveDistrictIndex(district);

  const bool ok = tryBeginSta(ssid, pass, WIFI_CONNECT_TIMEOUT_MS * 2, true);
  if (ok) {
    saveCredentials(String(ssid), String(pass));
    portalSuccess = true;
    String resp = "{\"ok\":true,\"ip\":\"";
    resp += WiFi.localIP().toString();
    resp += "\"}";
    sendJson(200, resp.c_str());
    return;
  }

  WiFi.mode(WIFI_AP_STA);
  ensureSoftAp();
  sendOkFalse(200, "連接失敗，請檢查密碼或訊號");
}

static void handleTouchStatus() {
  const String json = touchCalStatusJson();
  sendJson(200, json.c_str());
}

static void handleTouchCalStart() {
  if (touchCalStartCb) {
    touchCalStartCb();
  }
  sendJsonOk();
}

static void handleTouchCalReset() {
  if (touchCalResetCb) {
    touchCalResetCb();
  } else {
    touchCalClear();
  }
  sendJsonOk();
}

static void handleTouchCalDone() {
  if (touchCalDoneCb) {
    touchCalDoneCb();
  } else {
    touchCalCancel();
  }
  sendJsonOk();
}

static void handleCaptive() {
  server.sendHeader("Location", String("http://") + AP_IP.toString() + "/", true);
  server.send(302, "text/plain", "");
}

static void setupCommonRoutes() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/config", HTTP_GET, handleConfig);
  server.on("/districts", HTTP_GET, handleDistricts);
  server.on("/district", HTTP_POST, handleDistrictPost);
  server.on("/scan", HTTP_GET, handleScan);
  server.on("/connect", HTTP_POST, handleConnect);
  server.on("/touch/status", HTTP_GET, handleTouchStatus);
  server.on("/touch/cal/start", HTTP_POST, handleTouchCalStart);
  server.on("/touch/cal/reset", HTTP_POST, handleTouchCalReset);
  server.on("/touch/cal/done", HTTP_POST, handleTouchCalDone);
  server.onNotFound(handleRoot);
}

static void stopPortalServices() {
  if (scanInProgress) {
    WiFi.scanDelete();
    scanInProgress = false;
  }
  server.stop();
  dnsServer.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
}

bool wifiStartConfigPortal(void (*onStatus)(const char *title, const char *detail)) {
  portalSuccess = false;
  portalModeAp = true;
  settingsServerRunning = false;
  scanInProgress = false;
  lastScanMs = 0;
  scanJson = "[]";

  WiFi.persistent(false);
  WiFi.setSleep(false);
  WiFi.disconnect(true, true);
  delay(200);
  WiFi.mode(WIFI_AP_STA);
  ensureSoftAp();
  delay(100);

  dnsServer.start(53, "*", AP_IP);
  setupCommonRoutes();
  server.on("/generate_204", HTTP_GET, handleCaptive);
  server.on("/hotspot-detect.html", HTTP_GET, handleCaptive);
  server.on("/connecttest.txt", HTTP_GET, handleCaptive);
  server.on("/fwlink", HTTP_GET, handleCaptive);
  server.begin();

  startAsyncScan();

  Serial.printf("[Wi-Fi] Portal AP=%s IP=%s\n", AP_SSID, AP_IP.toString().c_str());
  if (onStatus) {
    char detail[120];
    snprintf(detail, sizeof(detail), "請連接 %s 後開啟 %s", AP_SSID,
             wifiPortalUrl().c_str());
    onStatus("Wi-Fi 設定", detail);
  }

  uint32_t lastStatusMs = millis();
  while (!portalSuccess) {
    pollScanComplete();
    dnsServer.processNextRequest();
    server.handleClient();

    if (millis() - lastStatusMs > 30000) {
      lastStatusMs = millis();
      // 只打日誌，避免 onStatus→fillScreen 造成設定期間週期閃屏
      Serial.printf("[Wi-Fi] portal waiting, connect %s → %s\n", AP_SSID,
                    wifiPortalUrl().c_str());
    }

    delay(2);
    yield();
  }

  delay(800);
  stopPortalServices();
  if (onStatus) {
    onStatus("啟動中", "Wi-Fi 已連接");
  }
  return WiFi.status() == WL_CONNECTED;
}

void wifiStartSettingsServer() {
  if (settingsServerRunning || WiFi.status() != WL_CONNECTED) {
    return;
  }

  portalModeAp = false;
  setupCommonRoutes();
  server.begin();
  settingsServerRunning = true;
  Serial.printf("[Wi-Fi] Settings http://%s/\n", WiFi.localIP().toString().c_str());
}

void wifiHandleClient() {
  if (settingsServerRunning) {
    server.handleClient();
  }
}

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>
#include <U8g2_for_TFT_eSPI.h>
#include "wifi_config.h"
#include "weather_config.h"
#include "font_render.h"

TFT_eSPI tft = TFT_eSPI();
U8g2_for_TFT_eSPI u8g2;

struct WiFiNetwork {
  const char *ssid;
  const char *password;
};

static const WiFiNetwork WIFI_NETWORKS[] = {
    {WIFI_1_SSID, WIFI_1_PASSWORD},
    {WIFI_2_SSID, WIFI_2_PASSWORD},
};

static const size_t WIFI_NETWORK_COUNT = sizeof(WIFI_NETWORKS) / sizeof(WIFI_NETWORKS[0]);
static int preferredNetworkIndex = -1;
static uint32_t lastWeatherFetchMs = 0;
static bool weatherReady = false;
static int lineHeight = 16;

static const uint8_t TFT_ROTATION = 3;  // landscape 320x240
static const int SCREEN_W = 320;
static const int SCREEN_H = 240;
static const int PAD_X = 8;
static const int LEFT_W = 108;
static const int RIGHT_X = PAD_X + LEFT_W + 6;
static const int RIGHT_W = SCREEN_W - RIGHT_X - PAD_X;

static const uint16_t COLOR_BG = TFT_NAVY;
static const uint16_t COLOR_DIVIDER = 0x4208;
static const uint16_t COLOR_LABEL = TFT_CYAN;
static const uint16_t COLOR_MUTED = TFT_DARKGREY;

struct WeatherLayout {
  int lineHeight;
  int contentTop;
  int footerY;
  int warningY;
  int warningH;
  int forecastLabelY;
  int forecastY;
  int forecastLinesPerPage;
};

#if UI_FONT == FONT_WQY12_CHINESE2
static const uint8_t *TEXT_FONT = u8g2_font_wqy12_t_chinese2;
#elif UI_FONT == FONT_WQY16_CHINESE2
static const uint8_t *TEXT_FONT = u8g2_font_wqy16_t_chinese2;
#elif UI_FONT == FONT_UNIFONT_CHINESE2
static const uint8_t *TEXT_FONT = u8g2_font_unifont_t_chinese2;
#elif UI_FONT == FONT_UNIFONT_CHINESE3
static const uint8_t *TEXT_FONT = u8g2_font_unifont_t_chinese3;
#elif UI_FONT == FONT_WQY12_GB2312B
static const uint8_t *TEXT_FONT = u8g2_font_wqy12_t_gb2312b;
#elif UI_FONT == FONT_SD_PFTC
static const uint8_t *TEXT_FONT = u8g2_font_unifont_t_chinese2;
#else
static const uint8_t *TEXT_FONT = u8g2_font_wqy14_t_chinese2;
#endif

struct WeatherData {
  String tempPlace;
  float temperature = NAN;
  int humidity = -1;
  float rainfallMm = NAN;
  int uvIndex = -1;
  String forecast;
  String warning;
};

static WeatherData weather;

bool fetchWeather();
void drawWeatherScreen();

WeatherLayout calcWeatherLayout() {
  WeatherLayout layout{};
  layout.lineHeight = lineHeight;
  layout.contentTop = 6;

  const bool hasWarning = weather.warning.length() > 0;
  layout.warningH = hasWarning ? layout.lineHeight + 10 : 0;
  layout.footerY = SCREEN_H - 4 - layout.warningH;
  layout.warningY = SCREEN_H - layout.warningH;
  layout.forecastLabelY = layout.contentTop;
  layout.forecastY = layout.forecastLabelY + layout.lineHeight + 4;

  const int forecastArea = layout.footerY - layout.forecastY - 4;
  layout.forecastLinesPerPage = max(1, forecastArea / layout.lineHeight);
  return layout;
}

bool isPlaceholderSsid(const char *ssid) {
  return ssid == nullptr || ssid[0] == '\0' ||
         String(ssid).startsWith("YOUR_WIFI_SSID");
}

bool isConfiguredNetwork(const WiFiNetwork &network) {
  return !isPlaceholderSsid(network.ssid) && network.password != nullptr;
}

static int forecastLineCount = 0;
static int forecastPage = 0;
static int forecastPageCount = 1;
static String forecastLines[24];
static uint32_t lastForecastPageMs = 0;

bool weatherDisplayChanged(const WeatherData &before, const WeatherData &after) {
  if (before.tempPlace != after.tempPlace) {
    return true;
  }
  if (isnan(before.temperature) != isnan(after.temperature)) {
    return true;
  }
  if (!isnan(before.temperature) &&
      fabsf(before.temperature - after.temperature) >= 0.1f) {
    return true;
  }
  if (before.humidity != after.humidity) {
    return true;
  }
  if (isnan(before.rainfallMm) != isnan(after.rainfallMm)) {
    return true;
  }
  if (!isnan(before.rainfallMm) && !isnan(after.rainfallMm) &&
      fabsf(before.rainfallMm - after.rainfallMm) >= 0.1f) {
    return true;
  }
  if (before.uvIndex != after.uvIndex) {
    return true;
  }
  if (before.forecast != after.forecast) {
    return true;
  }
  if (before.warning != after.warning) {
    return true;
  }
  return false;
}

bool updateWeatherDisplay() {
  const bool wasReady = weatherReady;
  const WeatherData previous = weather;
  const String previousForecast = weather.forecast;

  if (!fetchWeather()) {
    return false;
  }

  if (previousForecast != weather.forecast) {
    forecastPage = 0;
  }

  if (!wasReady || weatherDisplayChanged(previous, weather)) {
    drawWeatherScreen();
    Serial.println("[Weather] data changed, redraw");
  } else {
    Serial.println("[Weather] unchanged, skip redraw");
  }
  return true;
}

int wrapTextToLines(const String &text, String *lines, int maxLines, int maxWidth) {
  String remaining = text;
  int count = 0;

  while (remaining.length() > 0 && count < maxLines) {
    const int cut = fontUtf8WrapIndex(remaining, maxWidth);
    if (cut <= 0) {
      break;
    }
    lines[count++] = remaining.substring(0, cut);
    remaining = remaining.substring(cut);
  }

  return count;
}

void rebuildForecastLines() {
  forecastLineCount = wrapTextToLines(weather.forecast, forecastLines, 24, RIGHT_W);
  const WeatherLayout layout = calcWeatherLayout();
  const int linesPerPage = layout.forecastLinesPerPage;
  forecastPageCount = max(1, (forecastLineCount + linesPerPage - 1) / linesPerPage);
  if (forecastPage >= forecastPageCount) {
    forecastPage = 0;
  }
}

void drawWrappedText(const String &text, int x, int y, int maxWidth, int lineHeightPx,
                     uint16_t color, int maxLines, uint16_t bg = TFT_BLACK) {
  String lines[16];
  const int count = wrapTextToLines(text, lines, maxLines, maxWidth);

  for (int i = 0; i < count; i++) {
    fontDrawText(tft, x, y + i * lineHeightPx, lines[i].c_str(), color, bg);
  }
}

const char *wifiStatusText(wl_status_t status) {
  switch (status) {
    case WL_IDLE_STATUS:
      return "IDLE";
    case WL_NO_SSID_AVAIL:
      return "NO_SSID";
    case WL_SCAN_COMPLETED:
      return "SCAN_DONE";
    case WL_CONNECTED:
      return "CONNECTED";
    case WL_CONNECT_FAILED:
      return "CONNECT_FAILED";
    case WL_CONNECTION_LOST:
      return "LOST";
    case WL_DISCONNECTED:
      return "DISCONNECTED";
    default:
      return "UNKNOWN";
  }
}

void initDisplay() {
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, TFT_BACKLIGHT_ON);
  tft.init();
  tft.setRotation(TFT_ROTATION);
  tft.fillScreen(TFT_BLACK);
  initFontSystem(tft, u8g2, TEXT_FONT);
  lineHeight = fontLineHeight();
  if (usingSdFont()) {
    Serial.println("[Font] Using SD PingFang TC");
  } else {
    Serial.println("[Font] Using built-in fallback font");
  }
}

static int lastScanCount = -1;

bool isNetworkVisible(const char *ssid) {
  if (lastScanCount <= 0) {
    return true;
  }

  for (int i = 0; i < lastScanCount; i++) {
    if (WiFi.SSID(i) == ssid) {
      return true;
    }
  }
  return false;
}

void refreshWifiScan() {
  WiFi.scanDelete();
  lastScanCount = WiFi.scanNetworks(false, false);
  Serial.printf("[Wi-Fi] scan found %d networks\n", lastScanCount);
}

bool tryConnectNetwork(int index) {
  const WiFiNetwork &network = WIFI_NETWORKS[index];

  if (!isNetworkVisible(network.ssid)) {
    Serial.printf("[Wi-Fi %d] skip %s (not in scan)\n", index + 1, network.ssid);
    return false;
  }

  Serial.printf("[Wi-Fi %d] Connecting to %s\n", index + 1, network.ssid);

  WiFi.disconnect(false);
  delay(100);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(network.ssid, network.password);

  const uint32_t startMs = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startMs < WIFI_CONNECT_TIMEOUT_MS) {
    const wl_status_t status = WiFi.status();
    if (status == WL_NO_SSID_AVAIL && millis() - startMs > WIFI_NO_SSID_FAIL_MS) {
      Serial.printf("[Wi-Fi %d] abort: NO_SSID\n", index + 1);
      break;
    }
    if (status == WL_CONNECT_FAILED && millis() - startMs > WIFI_NO_SSID_FAIL_MS) {
      Serial.printf("[Wi-Fi %d] abort: CONNECT_FAILED\n", index + 1);
      break;
    }
    delay(250);
    Serial.print(".");
    yield();
  }
  Serial.println();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.printf("[Wi-Fi %d] failed: %s (%d)\n", index + 1,
                  wifiStatusText(WiFi.status()), WiFi.status());
    return false;
  }

  preferredNetworkIndex = index;
  Serial.printf("[Wi-Fi %d] IP: %s\n", index + 1, WiFi.localIP().toString().c_str());
  return true;
}

bool connectWiFi() {
  refreshWifiScan();

  int tryOrder[WIFI_NETWORK_COUNT];
  size_t tryCount = 0;

  if (preferredNetworkIndex >= 0 && preferredNetworkIndex < (int)WIFI_NETWORK_COUNT &&
      isConfiguredNetwork(WIFI_NETWORKS[preferredNetworkIndex])) {
    tryOrder[tryCount++] = preferredNetworkIndex;
  }

  for (size_t i = 0; i < WIFI_NETWORK_COUNT; i++) {
    if (!isConfiguredNetwork(WIFI_NETWORKS[i])) {
      continue;
    }
    bool alreadyListed = false;
    for (size_t j = 0; j < tryCount; j++) {
      if (tryOrder[j] == (int)i) {
        alreadyListed = true;
        break;
      }
    }
    if (!alreadyListed) {
      tryOrder[tryCount++] = i;
    }
  }

  for (size_t i = 0; i < tryCount; i++) {
    if (tryConnectNetwork(tryOrder[i])) {
      return true;
    }
  }

  return false;
}

String buildApiUrl(const char *dataType) {
  return String(HKO_API_BASE) + "?dataType=" + dataType + "&lang=" + HKO_LANG;
}

String httpGet(const String &url) {
  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.setTimeout(WEATHER_HTTP_TIMEOUT_MS);
  if (!http.begin(client, url)) {
    return "";
  }

  const int code = http.GET();
  String payload;
  if (code == HTTP_CODE_OK) {
    payload = http.getString();
  } else {
    Serial.printf("HTTP %d for %s\n", code, url.c_str());
  }

  http.end();
  return payload;
}

bool pickDistrictTemperature(JsonObject temperatureRoot, String &placeOut, float &valueOut) {
  JsonArray data = temperatureRoot["data"].as<JsonArray>();
  if (data.isNull()) {
    return false;
  }

  for (JsonObject item : data) {
    const char *place = item["place"] | "";
    if (strcmp(place, WEATHER_TEMP_STATION) == 0) {
      placeOut = WEATHER_DISTRICT_LABEL;
      valueOut = item["value"] | NAN;
      return !isnan(valueOut);
    }
  }

  JsonObject first = data[0];
  if (first.isNull()) {
    return false;
  }
  placeOut = WEATHER_DISTRICT_LABEL;
  valueOut = first["value"] | NAN;
  return !isnan(valueOut);
}

bool pickDistrictRainfall(JsonObject rainfallRoot, float &maxOut) {
  JsonArray data = rainfallRoot["data"].as<JsonArray>();
  if (data.isNull()) {
    return false;
  }

  for (JsonObject item : data) {
    const char *place = item["place"] | "";
    if (strcmp(place, WEATHER_RAINFALL_PLACE) == 0) {
      maxOut = item["max"] | NAN;
      return !isnan(maxOut);
    }
  }
  return false;
}

bool pickHumidity(JsonObject humidityRoot, int &valueOut) {
  JsonArray data = humidityRoot["data"].as<JsonArray>();
  if (data.isNull() || data.size() == 0) {
    return false;
  }

  for (JsonObject item : data) {
    const char *place = item["place"] | "";
    if (strcmp(place, "香港天文台") == 0 || strcmp(place, "京士柏") == 0) {
      valueOut = item["value"] | -1;
      return valueOut >= 0;
    }
  }

  valueOut = data[0]["value"] | -1;
  return valueOut >= 0;
}

String joinActiveWarningNames(JsonObject warnsumRoot) {
  String result;
  for (JsonPair kv : warnsumRoot) {
    JsonObject item = kv.value().as<JsonObject>();
    if (item.isNull()) {
      continue;
    }
    const char *name = item["name"] | "";
    if (name[0] == '\0') {
      continue;
    }
    if (result.length() > 0) {
      result += " ";
    }
    result += name;
  }
  return result;
}

const char *uvLevelText(int uv) {
  if (uv < 0) {
    return "--";
  }
  if (uv <= 2) {
    return "低";
  }
  if (uv <= 5) {
    return "中等";
  }
  if (uv <= 7) {
    return "高";
  }
  if (uv <= 10) {
    return "甚高";
  }
  return "極高";
}

void drawWeatherLeftPanel(const WeatherLayout &layout) {
  const int x = PAD_X;
  int y = layout.contentTop;

  char tempLine[16];
  snprintf(tempLine, sizeof(tempLine), "%.0f°C", weather.temperature);
  fontDrawText(tft, x, y, tempLine, TFT_YELLOW, COLOR_BG);
  y += layout.lineHeight + 2;

  char placeLine[32];
  snprintf(placeLine, sizeof(placeLine), "%s", weather.tempPlace.c_str());
  fontDrawText(tft, x, y, placeLine, TFT_WHITE, COLOR_BG);
  y += layout.lineHeight + 2;

  char humidityLine[20];
  snprintf(humidityLine, sizeof(humidityLine), "濕 %d%%", weather.humidity);
  fontDrawText(tft, x, y, humidityLine, TFT_WHITE, COLOR_BG);
  y += layout.lineHeight + 2;

  if (!isnan(weather.rainfallMm)) {
    char rainLine[20];
    snprintf(rainLine, sizeof(rainLine), "雨 %.0fmm", weather.rainfallMm);
    fontDrawText(tft, x, y, rainLine, TFT_WHITE, COLOR_BG);
    y += layout.lineHeight + 2;
  }

  char uvLine[24];
  snprintf(uvLine, sizeof(uvLine), "UV%d %s", weather.uvIndex, uvLevelText(weather.uvIndex));
  fontDrawText(tft, x, y, uvLine, TFT_WHITE, COLOR_BG);

  tft.drawFastVLine(RIGHT_X - 3, layout.contentTop, layout.footerY - layout.contentTop,
                    COLOR_DIVIDER);
}

void drawWeatherForecast(const WeatherLayout &layout) {
  fontDrawText(tft, RIGHT_X, layout.forecastLabelY, "天氣預報", COLOR_LABEL, COLOR_BG);

  if (forecastPageCount > 1) {
    char pageInfo[12];
    snprintf(pageInfo, sizeof(pageInfo), "%d/%d", forecastPage + 1, forecastPageCount);
    fontDrawTextRight(tft, SCREEN_W - PAD_X, layout.forecastLabelY, pageInfo, COLOR_MUTED,
                      COLOR_BG);
  }

  const int startLine = forecastPage * layout.forecastLinesPerPage;
  for (int i = 0; i < layout.forecastLinesPerPage; i++) {
    const int lineIndex = startLine + i;
    if (lineIndex >= forecastLineCount) {
      break;
    }
    fontDrawText(tft, RIGHT_X, layout.forecastY + i * layout.lineHeight,
                 forecastLines[lineIndex].c_str(), TFT_WHITE, COLOR_BG);
  }
}

void drawWeatherWarning(const WeatherLayout &layout) {
  if (weather.warning.length() == 0) {
    return;
  }

  tft.fillRect(0, layout.warningY, SCREEN_W, layout.warningH, TFT_MAROON);
  drawWrappedText(weather.warning, PAD_X, layout.warningY + 3, SCREEN_W - PAD_X * 2,
                  layout.lineHeight, TFT_WHITE, 1, TFT_MAROON);
}

bool fetchWeather() {
  weather = WeatherData{};

  const String currentPayload = httpGet(buildApiUrl("rhrread"));
  if (currentPayload.isEmpty()) {
    return false;
  }

  JsonDocument currentDoc;
  const DeserializationError err = deserializeJson(currentDoc, currentPayload);
  if (err) {
    Serial.printf("JSON error: %s\n", err.c_str());
    return false;
  }

  if (!pickDistrictTemperature(currentDoc["temperature"].as<JsonObject>(), weather.tempPlace,
                               weather.temperature)) {
    return false;
  }

  pickHumidity(currentDoc["humidity"].as<JsonObject>(), weather.humidity);
  pickDistrictRainfall(currentDoc["rainfall"].as<JsonObject>(), weather.rainfallMm);

  JsonArray uvData = currentDoc["uvindex"]["data"].as<JsonArray>();
  if (!uvData.isNull() && uvData.size() > 0) {
    weather.uvIndex = uvData[0]["value"] | -1;
  }

  const String warnsumPayload = httpGet(buildApiUrl("warnsum"));
  if (!warnsumPayload.isEmpty()) {
    JsonDocument warnsumDoc;
    const DeserializationError warnsumErr = deserializeJson(warnsumDoc, warnsumPayload);
    if (!warnsumErr) {
      weather.warning = joinActiveWarningNames(warnsumDoc.as<JsonObject>());
    }
  }

  const String forecastPayload = httpGet(buildApiUrl("flw"));
  if (!forecastPayload.isEmpty()) {
    JsonDocument forecastDoc;
    const DeserializationError err = deserializeJson(forecastDoc, forecastPayload);
    if (!err) {
      weather.forecast = forecastDoc["forecastDesc"] | "";
    }
  }

  if (weather.forecast.isEmpty()) {
    weather.forecast = "暫時無法取得本港天氣預測。";
  }

  weatherReady = true;
  return true;
}

void drawWeatherScreen() {
  rebuildForecastLines();
  const WeatherLayout layout = calcWeatherLayout();

  tft.fillScreen(COLOR_BG);
  drawWeatherLeftPanel(layout);
  drawWeatherForecast(layout);
  drawWeatherWarning(layout);
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("ESP32 HKO Weather - boot");

  initDisplay();
  tft.fillScreen(COLOR_BG);

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);

  if (connectWiFi() && updateWeatherDisplay()) {
    lastWeatherFetchMs = millis();
  }
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    static uint32_t lastRetryMs = 0;
    if (millis() - lastRetryMs >= WIFI_RETRY_INTERVAL_MS) {
      lastRetryMs = millis();
      Serial.println("[Wi-Fi] background reconnect");
      if (connectWiFi()) {
        if (updateWeatherDisplay()) {
          lastWeatherFetchMs = millis();
        }
      }
    }
    return;
  }

  if (millis() - lastWeatherFetchMs >= WEATHER_REFRESH_MS) {
    lastWeatherFetchMs = millis();
    updateWeatherDisplay();
    return;
  }

  if (weatherReady && forecastPageCount > 1 &&
      millis() - lastForecastPageMs >= 8000) {
    lastForecastPageMs = millis();
    forecastPage = (forecastPage + 1) % forecastPageCount;
    drawWeatherScreen();
  }
}

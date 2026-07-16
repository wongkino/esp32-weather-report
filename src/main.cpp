#include <Arduino.h>
#include <math.h>
#include <string.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>
#include <U8g2_for_TFT_eSPI.h>
#include "wifi_config.h"
#include "wifi_portal.h"
#include "weather_config.h"
#include "weather_warnings.h"
#include "font_render.h"
#include "districts.h"
#include "touch_cyd.h"

TFT_eSPI tft = TFT_eSPI();
U8g2_for_TFT_eSPI u8g2;

static int districtIndex = 0;
static uint32_t lastTouchMs = 0;
static bool touchTestMode = false;
static bool touchCalibrating = false;
static int16_t lastTouchX = -1;
static int16_t lastTouchY = -1;

static uint32_t lastWeatherFetchMs = 0;
static bool weatherReady = false;
static int lineHeight = 16;
static int smallLineHeight = 14;

static const uint8_t TFT_ROTATION = 0;  // portrait 240x320
static const int SCREEN_W = 240;
static const int SCREEN_H = 320;
static const int PAD_X = 8;
static const int CONTENT_W = SCREEN_W - PAD_X * 2;

static const uint16_t COLOR_BG = TFT_BLACK;
static const uint16_t COLOR_TEXT = TFT_WHITE;
static const uint16_t COLOR_MUTED = 0x8410;      // mid grey
static const uint16_t COLOR_TEMP_LOW = 0x7DFF;   // light blue
static const uint16_t COLOR_TEMP_HIGH = 0xFD20;  // orange
static const uint16_t COLOR_WARN_BG = TFT_RED;
static const uint16_t COLOR_WARN_FG = TFT_WHITE;

struct WeatherLayout {
  int mainLineHeight;
  int detailLineHeight;
  int contentTop;
  int tempY;
  int metricsY;
  int forecastY;
  int updateY;
  int warningY;
  int warningH;
  int forecastLinesPerPage;
};

// SD 字型失敗時的 U8g2 fallback（字集有限，僅狀態畫面）
static const uint8_t *TEXT_FONT = u8g2_font_unifont_t_chinese2;

struct WeatherData {
  float temperature = NAN;
  float tempMin = NAN;
  float tempMax = NAN;
  int humidity = -1;
  String forecast;
  String updateLabel;
  String warning;
};

static WeatherData weather;

static bool fetchWeather();
static bool updateWeatherDisplay();
static void drawWeatherScreen();
static void drawCurrentPage();
static void drawFooterOnly();
static void drawStatusScreen(const char *title, const char *detail);
static void drawTouchTestScreen();

static const WeatherDistrict &currentDistrict() {
  districtIndex = clampDistrictIndex(districtIndex);
  return WEATHER_DISTRICTS[districtIndex];
}

static void loadDistrictPreference() {
  districtIndex = wifiLoadDistrictIndex();
  Serial.printf("[District] loaded idx=%d %s\n", districtIndex, currentDistrict().label);
}

static void onDistrictChangedFromWeb(int index) {
  districtIndex = index;
  Serial.printf("[District] web change idx=%d %s\n", districtIndex, currentDistrict().label);
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }
  drawStatusScreen("啟動中", "正在更新天氣…");
  if (updateWeatherDisplay()) {
    lastWeatherFetchMs = millis();
  } else if (weatherReady) {
    drawCurrentPage();
  }
}

static void onTouchCalStartFromWeb() {
  touchTestMode = true;
  touchCalibrating = true;
  lastTouchX = -1;
  touchCalStart();
  drawTouchTestScreen();
  Serial.println("[Touch] calibration started from web");
}

static void onTouchCalResetFromWeb() {
  touchCalClear();
  touchTestMode = false;
  touchCalibrating = false;
  lastTouchX = -1;
  if (weatherReady) {
    drawCurrentPage();
  }
  Serial.println("[Touch] reset to default from web");
}

static void onTouchCalDoneFromWeb() {
  touchCalCancel();
  touchTestMode = false;
  touchCalibrating = false;
  lastTouchX = -1;
  if (weatherReady) {
    drawCurrentPage();
  } else {
    drawStatusScreen("啟動中", nullptr);
  }
  Serial.println("[Touch] done from web");
}

static int forecastLineCount = 0;
static String forecastLines[32];
static String lastWrappedForecast;

// 快取各字型行高，避免每次排版都輪流 SD loadFont
static int cachedMainH = 0;
static int cachedSmallH = 0;
static int cachedDetailH = 0;
static int cachedLargeH = 0;

static void cacheFontHeights() {
  fontUseMain();
  cachedMainH = fontLineHeight();
  fontUseSmall();
  cachedSmallH = fontLineHeight();
  fontUseDetail();
  cachedDetailH = fontLineHeight();
  fontUseLarge();
  cachedLargeH = fontLineHeight();
  fontUseMain();
}

static WeatherLayout calcWeatherLayout() {
  WeatherLayout layout{};

  if (cachedMainH <= 0) {
    cacheFontHeights();
  }
  const int mainH = cachedMainH;
  const int detailH = cachedDetailH;
  const int largeH = cachedLargeH;

  layout.mainLineHeight = mainH;
  layout.detailLineHeight = detailH;

  const bool hasWarning = weather.warning.length() > 0;
  layout.warningH = hasWarning ? mainH + 10 : 0;
  layout.warningY = 0;

  layout.contentTop = layout.warningH + 8;
  layout.tempY = layout.contentTop;
  layout.metricsY = layout.tempY + 4;

  int metricsLines = 1;
  if (!isnan(weather.tempMin)) {
    metricsLines++;
  }
  if (!isnan(weather.tempMax)) {
    metricsLines++;
  }
  const int metricsBottom = layout.metricsY + metricsLines * (mainH + 2);
  const int tempBottom = layout.tempY + largeH;
  layout.forecastY = max(tempBottom, metricsBottom) + 10;
  layout.updateY = SCREEN_H - detailH - 4;  // 頁尾 6pt

  const int forecastArea = layout.updateY - layout.forecastY - 6;
  layout.forecastLinesPerPage = max(1, forecastArea / detailH);
  return layout;
}

static bool weatherDisplayChanged(const WeatherData &before, const WeatherData &after) {
  if (isnan(before.temperature) != isnan(after.temperature)) {
    return true;
  }
  if (!isnan(before.temperature) &&
      fabsf(before.temperature - after.temperature) >= 0.1f) {
    return true;
  }
  if (isnan(before.tempMin) != isnan(after.tempMin) ||
      isnan(before.tempMax) != isnan(after.tempMax)) {
    return true;
  }
  if (!isnan(before.tempMin) && !isnan(after.tempMin) &&
      fabsf(before.tempMin - after.tempMin) >= 0.1f) {
    return true;
  }
  if (!isnan(before.tempMax) && !isnan(after.tempMax) &&
      fabsf(before.tempMax - after.tempMax) >= 0.1f) {
    return true;
  }
  if (before.humidity != after.humidity) {
    return true;
  }
  if (before.forecast != after.forecast) {
    return true;
  }
  // 略過 updateLabel 單獨變更，避免每輪刷新整屏閃爍
  if (before.warning != after.warning) {
    return true;
  }
  return false;
}

static bool updateWeatherDisplay() {
  const bool wasReady = weatherReady;
  const WeatherData previous = weather;

  if (!fetchWeather()) {
    return false;
  }

  if (!wasReady || weatherDisplayChanged(previous, weather)) {
    if (!touchTestMode) {
      drawWeatherScreen();
    }
    Serial.println("[Weather] data changed, redraw");
  } else if (!touchTestMode && previous.updateLabel != weather.updateLabel) {
    drawFooterOnly();
    Serial.println("[Weather] footer only");
  } else {
    Serial.println("[Weather] unchanged, skip redraw");
  }
  return true;
}

static int wrapTextToLines(const String &text, String *lines, int maxLines, int maxWidth) {
  int count = 0;
  int start = 0;

  while (start < (int)text.length() && count < maxLines) {
    const int cut = fontUtf8WrapIndex(text, start, maxWidth);
    if (cut <= start) {
      break;
    }
    lines[count++] = text.substring(start, cut);
    start = cut;
  }

  return count;
}

static void rebuildForecastLines() {
  if (weather.forecast == lastWrappedForecast && forecastLineCount > 0) {
    return;
  }
  fontUseDetail();
  forecastLineCount = wrapTextToLines(weather.forecast, forecastLines, 32, CONTENT_W);
  lastWrappedForecast = weather.forecast;
}

static void drawWrappedText(const String &text, int x, int y, int maxWidth, int lineHeightPx,
                            uint16_t color, int maxLines, uint16_t bg = TFT_BLACK) {
  int start = 0;
  for (int i = 0; i < maxLines && start < (int)text.length(); i++) {
    const int cut = fontUtf8WrapIndex(text, start, maxWidth);
    if (cut <= start) {
      break;
    }
    const String line = text.substring(start, cut);
    fontDrawText(tft, x, y + i * lineHeightPx, line.c_str(), color, bg);
    start = cut;
  }
}

static void initDisplay() {
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, TFT_BACKLIGHT_ON);
  tft.init();
  tft.setRotation(TFT_ROTATION);
  tft.fillScreen(TFT_BLACK);
  initFontSystem(tft, u8g2, TEXT_FONT);
  cacheFontHeights();
  lineHeight = cachedMainH;
  smallLineHeight = cachedSmallH;
  if (usingSdFont()) {
    Serial.println("[Font] Using SD PingFang TC tiers");
    Serial.printf("[Font] detail=%d small=%d large=%d\n", usingSdDetailFont() ? 1 : 0,
                  usingSdSmallFont() ? 1 : 0, usingSdLargeFont() ? 1 : 0);
  } else {
    Serial.println("[Font] Using built-in fallback font");
  }
}

static void onWifiPortalStatus(const char *title, const char *detail) {
  drawStatusScreen(title, detail);
}

// 先試 NVS 已存憑證；失敗則開 AP 網頁設定直到連上
static bool ensureWifiConnected() {
  if (wifiIsConnected()) {
    return true;
  }

  drawStatusScreen("啟動中", "正在連接 Wi-Fi…");
  if (wifiTryConnect()) {
    return true;
  }

  Serial.println("[Wi-Fi] starting config portal");
  return wifiStartConfigPortal(onWifiPortalStatus);
}

static String buildApiUrl(const char *dataType) {
  return String(HKO_API_BASE) + "?dataType=" + dataType + "&lang=" + HKO_LANG;
}

static bool httpGetJson(const char *dataType, JsonDocument &doc) {
  const String url = buildApiUrl(dataType);
  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.setTimeout(WEATHER_HTTP_TIMEOUT_MS);
  if (!http.begin(client, url)) {
    Serial.printf("[Weather] HTTP begin failed: %s\n", dataType);
    return false;
  }

  const int code = http.GET();
  if (code != HTTP_CODE_OK) {
    Serial.printf("[Weather] HTTP %d for %s\n", code, url.c_str());
    http.end();
    return false;
  }

  const DeserializationError err = deserializeJson(doc, http.getStream());
  http.end();
  if (err) {
    Serial.printf("[Weather] JSON error for %s: %s\n", dataType, err.c_str());
    return false;
  }
  return true;
}

static bool pickDistrictTemperature(JsonObject temperatureRoot, float &valueOut) {
  JsonArray data = temperatureRoot["data"].as<JsonArray>();
  if (data.isNull()) {
    return false;
  }

  const char *wanted = currentDistrict().tempStation;
  for (JsonObject item : data) {
    const char *place = item["place"] | "";
    if (strcmp(place, wanted) == 0) {
      valueOut = item["value"] | NAN;
      return !isnan(valueOut);
    }
  }

  JsonObject first = data[0];
  if (first.isNull()) {
    return false;
  }
  const char *fallback = first["place"] | "?";
  Serial.printf("[Weather] station '%s' missing, fallback '%s'\n", wanted, fallback);
  valueOut = first["value"] | NAN;
  return !isnan(valueOut);
}

static bool pickHumidity(JsonObject humidityRoot, int &valueOut) {
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

static String formatUpdateLabel(const char *isoTime) {
  // 2026-07-16T09:45:00+08:00
  if (isoTime == nullptr || strlen(isoTime) < 16) {
    return "";
  }
  const int hour = (isoTime[11] - '0') * 10 + (isoTime[12] - '0');
  const int minute = (isoTime[14] - '0') * 10 + (isoTime[15] - '0');
  const char *period = hour < 12 ? "上午" : "下午";
  int hour12 = hour % 12;
  if (hour12 == 0) {
    hour12 = 12;
  }
  char buf[40];
  snprintf(buf, sizeof(buf), "天文台更新 %s%d:%02d", period, hour12, minute);
  return String(buf);
}

static void drawHeaderAndTemp(const WeatherLayout &layout) {
  char tempLine[16];
  if (isnan(weather.temperature)) {
    snprintf(tempLine, sizeof(tempLine), "--°C");
  } else {
    snprintf(tempLine, sizeof(tempLine), "%.0f°C", weather.temperature);
  }

  fontUseLarge();
  fontDrawText(tft, PAD_X, layout.tempY, tempLine, COLOR_TEXT, COLOR_BG);
  const int metricsX = min(PAD_X + fontTextWidth(tempLine) + 10, SCREEN_W - 100);
  fontUseMain();

  int my = layout.metricsY;
  char line[24];

  if (!isnan(weather.tempMin)) {
    snprintf(line, sizeof(line), "低%.0f°", weather.tempMin);
    fontDrawText(tft, metricsX, my, line, COLOR_TEMP_LOW, COLOR_BG);
    my += layout.mainLineHeight + 2;
  }
  if (!isnan(weather.tempMax)) {
    snprintf(line, sizeof(line), "高%.0f°", weather.tempMax);
    fontDrawText(tft, metricsX, my, line, COLOR_TEMP_HIGH, COLOR_BG);
    my += layout.mainLineHeight + 2;
  }
  if (weather.humidity >= 0) {
    snprintf(line, sizeof(line), "濕度 %d%%", weather.humidity);
  } else {
    snprintf(line, sizeof(line), "濕度 --%%");
  }
  fontDrawText(tft, metricsX, my, line, COLOR_TEXT, COLOR_BG);
}

static void drawFooter(const WeatherLayout &layout) {
  fontUseDetail();
  char footer[72];
  footer[0] = '\0';
  if (WiFi.status() == WL_CONNECTED && weather.updateLabel.length() > 0) {
    snprintf(footer, sizeof(footer), "%s | %s", WiFi.localIP().toString().c_str(),
             weather.updateLabel.c_str());
  } else if (weather.updateLabel.length() > 0) {
    snprintf(footer, sizeof(footer), "%s", weather.updateLabel.c_str());
  } else if (WiFi.status() == WL_CONNECTED) {
    snprintf(footer, sizeof(footer), "%s", WiFi.localIP().toString().c_str());
  }
  if (footer[0] != '\0') {
    fontDrawTextRight(tft, SCREEN_W - PAD_X, layout.updateY, footer, COLOR_MUTED, COLOR_BG);
  }
}

static void drawFooterOnly() {
  if (!weatherReady || touchTestMode) {
    return;
  }
  const WeatherLayout layout = calcWeatherLayout();
  const int top = max(0, layout.updateY - 2);
  tft.fillRect(0, top, SCREEN_W, SCREEN_H - top, COLOR_BG);
  drawFooter(layout);
}

static void drawForecastBlock(const WeatherLayout &layout) {
  fontUseDetail();

  const int maxLines = layout.forecastLinesPerPage;
  const bool truncated = forecastLineCount > maxLines;
  const int drawLines = truncated ? maxLines : forecastLineCount;
  const int ellipsisW = fontTextWidth("…");

  for (int i = 0; i < drawLines; i++) {
    const char *src = forecastLines[i].c_str();
    char line[96];
    strncpy(line, src, sizeof(line) - 1);
    line[sizeof(line) - 1] = '\0';

    if (truncated && i == drawLines - 1) {
      while (line[0] != '\0' && fontTextWidth(line) + ellipsisW > CONTENT_W) {
        int prev = (int)strlen(line) - 1;
        while (prev > 0 && (line[prev] & 0xC0) == 0x80) {
          prev--;
        }
        if (prev <= 0) {
          line[0] = '\0';
          break;
        }
        line[prev] = '\0';
      }
      const size_t n = strlen(line);
      if (n + 3 < sizeof(line)) {
        // UTF-8 ellipsis …
        line[n] = (char)0xE2;
        line[n + 1] = (char)0x80;
        line[n + 2] = (char)0xA6;
        line[n + 3] = '\0';
      }
    }
    fontDrawText(tft, PAD_X, layout.forecastY + i * layout.detailLineHeight, line, COLOR_TEXT,
                 COLOR_BG);
  }

  drawFooter(layout);
}

static void drawWeatherWarning(const WeatherLayout &layout) {
  if (weather.warning.length() == 0 || layout.warningH <= 0) {
    return;
  }

  fontUseMain();
  tft.fillRect(0, layout.warningY, SCREEN_W, layout.warningH, COLOR_WARN_BG);

  const int textW = fontTextWidth(weather.warning.c_str());
  const int textX = max(PAD_X, (SCREEN_W - textW) / 2);
  const int textY = layout.warningY + (layout.warningH - layout.mainLineHeight) / 2;
  fontDrawText(tft, textX, textY, weather.warning.c_str(), COLOR_WARN_FG, COLOR_WARN_BG);
}

static bool fetchWeather() {
  WeatherData next;

  JsonDocument currentDoc;
  if (!httpGetJson("rhrread", currentDoc)) {
    Serial.println("[Weather] rhrread empty");
    return false;
  }

  if (!pickDistrictTemperature(currentDoc["temperature"].as<JsonObject>(), next.temperature)) {
    Serial.println("[Weather] temperature station missing");
    return false;
  }

  pickHumidity(currentDoc["humidity"].as<JsonObject>(), next.humidity);

  const char *rhrUpdate = currentDoc["updateTime"] | "";
  next.updateLabel = formatUpdateLabel(rhrUpdate);

  {
    JsonDocument warnsumDoc;
    if (httpGetJson("warnsum", warnsumDoc)) {
      next.warning = joinActiveWarningNames(warnsumDoc.as<JsonObject>());
    }
  }

  {
    JsonDocument forecastDoc;
    if (httpGetJson("flw", forecastDoc)) {
      next.forecast = forecastDoc["forecastDesc"] | "";
      const char *flwUpdate = forecastDoc["updateTime"] | "";
      const String flwLabel = formatUpdateLabel(flwUpdate);
      if (flwLabel.length() > 0) {
        next.updateLabel = flwLabel;
      }
    }
  }

  if (next.forecast.isEmpty()) {
    next.forecast = "暫時無法取得本港天氣預測。";
  }

  {
    JsonDocument fndDoc;
    if (httpGetJson("fnd", fndDoc)) {
      JsonArray days = fndDoc["weatherForecast"].as<JsonArray>();
      if (!days.isNull() && days.size() > 0) {
        JsonObject day0 = days[0];
        next.tempMin = day0["forecastMintemp"]["value"] | NAN;
        next.tempMax = day0["forecastMaxtemp"]["value"] | NAN;
      }
    }
  }

  weather = next;
  weatherReady = true;
  return true;
}

static void drawStatusScreen(const char *title, const char *detail) {
  tft.fillScreen(COLOR_BG);
  fontUseMain();

  const int titleW = fontTextWidth(title);
  const int titleX = max(PAD_X, (SCREEN_W - titleW) / 2);
  const int titleY = SCREEN_H / 2 - lineHeight;
  fontDrawText(tft, titleX, titleY, title, COLOR_TEXT, COLOR_BG);

  if (detail != nullptr && detail[0] != '\0') {
    fontUseSmall();
    drawWrappedText(detail, PAD_X, titleY + lineHeight + 10, CONTENT_W, smallLineHeight,
                    COLOR_MUTED, 4, COLOR_BG);
    fontUseMain();
  }
}

static void drawTouchMarker(int16_t x, int16_t y) {
  tft.fillCircle(x, y, 8, TFT_YELLOW);
  tft.drawCircle(x, y, 12, TFT_WHITE);
  tft.drawFastHLine(x - 18, y, 36, TFT_YELLOW);
  tft.drawFastVLine(x, y - 18, 36, TFT_YELLOW);

  fontUseSmall();
  char buf[48];
  snprintf(buf, sizeof(buf), "觸控 %d,%d", x, y);
  tft.fillRect(0, 0, SCREEN_W, smallLineHeight + 8, COLOR_BG);
  fontDrawText(tft, PAD_X, 4, buf, TFT_YELLOW, COLOR_BG);
  fontUseMain();
}

static void drawTouchTestScreen() {
  tft.fillScreen(COLOR_BG);
  fontUseMain();

  if (touchCalibrating) {
    const char *title = "觸控校準";
    fontDrawText(tft, max(PAD_X, (SCREEN_W - fontTextWidth(title)) / 2), 8, title, COLOR_TEXT,
                 COLOR_BG);
    fontUseSmall();
    drawWrappedText(touchCalPrompt(), PAD_X, 8 + lineHeight + 6, CONTENT_W, smallLineHeight,
                    COLOR_MUTED, 2, COLOR_BG);

    int16_t tx, ty;
    touchCalTarget(touchCalStep(), tx, ty);
    tft.fillCircle(tx, ty, 10, TFT_RED);
    tft.drawCircle(tx, ty, 14, TFT_WHITE);
    tft.drawFastHLine(tx - 22, ty, 44, TFT_RED);
    tft.drawFastVLine(tx, ty - 22, 44, TFT_RED);

    char stepBuf[24];
    snprintf(stepBuf, sizeof(stepBuf), "%d / %d", touchCalStep() + 1, TOUCH_CAL_POINTS);
    fontDrawText(tft, PAD_X, SCREEN_H - smallLineHeight - 8, stepBuf, COLOR_MUTED, COLOR_BG);
    fontUseMain();
    return;
  }

  const char *title = "觸控驗證";
  fontDrawText(tft, max(PAD_X, (SCREEN_W - fontTextWidth(title)) / 2), 8, title, COLOR_TEXT,
               COLOR_BG);
  fontUseSmall();
  drawWrappedText("黃點應對準手指。連點右下角3次回天氣。", PAD_X, 8 + lineHeight + 6, CONTENT_W,
                  smallLineHeight, COLOR_MUTED, 3, COLOR_BG);

  const int r = 4;
  tft.fillCircle(20, 20, r, COLOR_MUTED);
  tft.fillCircle(SCREEN_W - 21, 20, r, COLOR_MUTED);
  tft.fillCircle(20, SCREEN_H - 21, r, COLOR_MUTED);
  tft.fillCircle(SCREEN_W - 21, SCREEN_H - 21, r, COLOR_MUTED);
  tft.fillCircle(SCREEN_W / 2, SCREEN_H / 2, r, COLOR_MUTED);
  fontUseMain();
}

static void drawWeatherScreen() {
  rebuildForecastLines();
  const WeatherLayout layout = calcWeatherLayout();

  tft.fillScreen(COLOR_BG);
  drawHeaderAndTemp(layout);
  drawForecastBlock(layout);
  drawWeatherWarning(layout);
}

static void drawCurrentPage() {
  if (touchTestMode) {
    drawTouchTestScreen();
    if (!touchCalibrating && lastTouchX >= 0) {
      drawTouchMarker(lastTouchX, lastTouchY);
    }
  } else {
    drawWeatherScreen();
  }
}

static bool handleTouch() {
  if (!touchTestMode) {
    return false;  // 天氣畫面無觸控操作，略過 SoftSPI 取樣
  }
  if (millis() - lastTouchMs < 320) {
    return false;
  }
  int16_t x, y;
  if (!touchReadScreen(x, y)) {
    return false;
  }
  lastTouchMs = millis();

  if (touchCalibrating) {
    if (touchCalAddSample()) {
      touchWaitRelease();
      if (touchCalStep() >= TOUCH_CAL_POINTS) {
        touchCalibrating = false;
        lastTouchX = -1;
      }
      drawTouchTestScreen();
      return true;
    }
    return false;
  }

  Serial.printf("[Touch] hit=%d,%d\n", x, y);
  lastTouchX = x;
  lastTouchY = y;
  drawTouchTestScreen();
  drawTouchMarker(x, y);
  if (x > SCREEN_W - 50 && y > SCREEN_H - 50) {
    static uint8_t cornerHits = 0;
    cornerHits++;
    if (cornerHits >= 3) {
      cornerHits = 0;
      touchTestMode = false;
      lastTouchX = -1;
      drawCurrentPage();
    }
  }
  touchWaitRelease();
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("ESP32 HKO Weather - boot");

  loadDistrictPreference();
  wifiSetDistrictChangedCallback(onDistrictChangedFromWeb);
  wifiSetTouchCalStartCallback(onTouchCalStartFromWeb);
  wifiSetTouchCalResetCallback(onTouchCalResetFromWeb);
  wifiSetTouchCalDoneCallback(onTouchCalDoneFromWeb);
  initDisplay();
  touchBegin();
  drawStatusScreen("啟動中", nullptr);

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);

  if (!ensureWifiConnected()) {
    Serial.println("[Wi-Fi] portal ended without connection");
    drawStatusScreen("Wi-Fi連接失敗", "請重新開機再設定。");
    return;
  }

  // Portal 連線流程可能已寫入新地區
  loadDistrictPreference();
  wifiStartSettingsServer();
  drawStatusScreen("啟動中", "正在取得天氣資料…");

  if (!updateWeatherDisplay()) {
    Serial.println("[Weather] boot fetch failed");
    drawStatusScreen("天氣資料失敗", "已連線，稍後自動重試天文台 API。");
  } else {
    lastWeatherFetchMs = millis();
  }

  touchTestMode = false;
  touchCalibrating = false;
  if (weatherReady) {
    drawCurrentPage();
  }
}

void loop() {
  wifiHandleClient();

  if (handleTouch()) {
    return;
  }

  if (touchTestMode) {
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    static uint32_t lastRetryMs = 0;
    if (millis() - lastRetryMs >= WIFI_RETRY_INTERVAL_MS) {
      lastRetryMs = millis();
      Serial.println("[Wi-Fi] background reconnect / portal");
      if (!weatherReady) {
        drawStatusScreen("啟動中", "正在重新連線…");
      }
      if (ensureWifiConnected()) {
        wifiStartSettingsServer();
        if (updateWeatherDisplay()) {
          lastWeatherFetchMs = millis();
        } else if (!weatherReady) {
          drawStatusScreen("天氣資料失敗", "已連線，稍後自動重試天文台 API。");
        }
      } else if (!weatherReady) {
        drawStatusScreen("Wi-Fi連接失敗", "請連接 esp32-weather 設定網路。");
      }
    }
    return;
  }

  if (millis() - lastWeatherFetchMs >= WEATHER_REFRESH_MS) {
    lastWeatherFetchMs = millis();
    if (!updateWeatherDisplay() && !weatherReady) {
      drawStatusScreen("天氣資料失敗", "已連線，稍後自動重試天文台 API。");
    }
  }
}

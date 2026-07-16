#include "font_render.h"
#include "font_config.h"

#include <SD.h>
#include <SPI.h>
#include <string.h>

static bool sdFontReady = false;
static bool sdDetailFontReady = false;
static bool sdSmallFontReady = false;
static bool sdLargeFontReady = false;
static FontRole currentRole = FONT_ROLE_MAIN;
static const char *largeFontName = SD_FONT_LARGE_NAME;
static const uint8_t *fallbackFontPtr = nullptr;
static U8g2_for_TFT_eSPI *u8g2Ptr = nullptr;
static TFT_eSPI *tftPtr = nullptr;
static SPIClass sdSPI(VSPI);

static int nextUtf8Index(const String &text, int index) {
  if (index >= (int)text.length()) {
    return index;
  }
  int next = index + 1;
  while (next < (int)text.length() && (text[next] & 0xC0) == 0x80) {
    next++;
  }
  return next;
}

static bool fontFileOk(const char *path) {
  if (!SD.exists(path)) {
    return false;
  }
  File fontFile = SD.open(path);
  if (!fontFile) {
    return false;
  }
  const uint32_t fontBytes = fontFile.size();
  fontFile.close();
  Serial.printf("[Font] Found %s (%u bytes)\n", path, fontBytes);
  if (fontBytes == 0 || fontBytes > SD_FONT_MAX_BYTES) {
    Serial.printf("[Font] Reject %s (size limit %u)\n", path, SD_FONT_MAX_BYTES);
    return false;
  }
  return true;
}

static void logSdCardInfo() {
  const uint8_t cardType = SD.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("[SD] No card detected");
    return;
  }

  const char *typeLabel = "UNKNOWN";
  if (cardType == CARD_MMC) {
    typeLabel = "MMC";
  } else if (cardType == CARD_SD) {
    typeLabel = "SDSC";
  } else if (cardType == CARD_SDHC) {
    typeLabel = "SDHC";
  }

  Serial.printf("[SD] Mounted OK, type=%s, size=%lluMB, used=%lluMB\n", typeLabel,
                SD.cardSize() / (1024ULL * 1024ULL),
                SD.usedBytes() / (1024ULL * 1024ULL));

  File root = SD.open("/");
  if (!root) {
    Serial.println("[SD] Failed to open root directory");
    return;
  }

  Serial.println("[SD] Root files:");
  File entry = root.openNextFile();
  while (entry) {
    Serial.printf("[SD]   /%s (%u bytes)\n", entry.name(), entry.size());
    entry.close();
    entry = root.openNextFile();
  }
  root.close();
}

static void loadSdFontByName(const char *name) {
  if (tftPtr == nullptr) {
    return;
  }
  tftPtr->unloadFont();
  tftPtr->loadFont(name, SD);
}

bool initFontSystem(TFT_eSPI &tft, U8g2_for_TFT_eSPI &u8g2, const uint8_t *fallbackFont) {
  tftPtr = &tft;
  u8g2Ptr = &u8g2;
  fallbackFontPtr = fallbackFont;
  sdFontReady = false;
  sdDetailFontReady = false;
  sdSmallFontReady = false;
  sdLargeFontReady = false;
  currentRole = FONT_ROLE_MAIN;
  largeFontName = SD_FONT_LARGE_NAME;

  sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  if (!SD.begin(SD_CS, sdSPI, 20000000)) {
    Serial.println("[Font] SD card mount failed, use fallback font");
    u8g2.begin(tft);
    return false;
  }

  logSdCardInfo();

  if (!fontFileOk(SD_FONT_FILE)) {
    Serial.printf("[Font] Missing/invalid %s, use fallback font\n", SD_FONT_FILE);
    u8g2.begin(tft);
    return false;
  }

  loadSdFontByName(SD_FONT_NAME);
  sdFontReady = true;
  Serial.printf("[Font] Loaded main %s\n", SD_FONT_FILE);

  if (fontFileOk(SD_FONT_DETAIL_FILE)) {
    sdDetailFontReady = true;
    Serial.printf("[Font] Detail ready %s\n", SD_FONT_DETAIL_FILE);
  } else {
    Serial.printf("[Font] No %s, detail -> small/main\n", SD_FONT_DETAIL_FILE);
  }

  if (fontFileOk(SD_FONT_SMALL_FILE)) {
    sdSmallFontReady = true;
    Serial.printf("[Font] Small ready %s\n", SD_FONT_SMALL_FILE);
  } else {
    Serial.printf("[Font] No %s, small -> main\n", SD_FONT_SMALL_FILE);
  }

  if (fontFileOk(SD_FONT_LARGE_FILE)) {
    sdLargeFontReady = true;
    largeFontName = SD_FONT_LARGE_NAME;
    Serial.printf("[Font] Large ready %s\n", SD_FONT_LARGE_FILE);
  } else {
    Serial.println("[Font] No large font, large -> main");
  }

  return true;
}

bool usingSdFont() {
  return sdFontReady;
}

bool usingSdDetailFont() {
  return sdDetailFontReady;
}

bool usingSdSmallFont() {
  return sdSmallFontReady;
}

bool usingSdLargeFont() {
  return sdLargeFontReady;
}

void fontUseMain() {
  if (currentRole == FONT_ROLE_MAIN) {
    return;  // 已載入，避免反覆 SD loadFont（極慢、造成閃屏）
  }
  currentRole = FONT_ROLE_MAIN;
  if (sdFontReady) {
    loadSdFontByName(SD_FONT_NAME);
  }
}

void fontUseDetail() {
  if (sdDetailFontReady) {
    if (currentRole == FONT_ROLE_DETAIL) {
      return;
    }
    currentRole = FONT_ROLE_DETAIL;
    loadSdFontByName(SD_FONT_DETAIL_NAME);
    return;
  }
  fontUseSmall();
}

void fontUseSmall() {
  if (!sdSmallFontReady) {
    fontUseMain();
    return;
  }
  if (currentRole == FONT_ROLE_SMALL) {
    return;
  }
  currentRole = FONT_ROLE_SMALL;
  loadSdFontByName(SD_FONT_SMALL_NAME);
}

void fontUseLarge() {
  if (!sdLargeFontReady) {
    fontUseMain();
    return;
  }
  if (currentRole == FONT_ROLE_LARGE) {
    return;
  }
  currentRole = FONT_ROLE_LARGE;
  loadSdFontByName(largeFontName);
}

int fontLineHeight() {
  if (sdFontReady && tftPtr != nullptr) {
    // 6pt 平滑字型行高容易偏小，多留間距避免預報重疊
    const int pad = (currentRole == FONT_ROLE_DETAIL) ? 6 : 2;
    return tftPtr->fontHeight() + pad;
  }
  if (u8g2Ptr != nullptr && fallbackFontPtr != nullptr) {
    u8g2Ptr->setFont(fallbackFontPtr);
    return u8g2Ptr->getFontAscent() - u8g2Ptr->getFontDescent() + 2;
  }
  return 14;
}

int fontTextWidth(const char *text) {
  if (text == nullptr) {
    return 0;
  }
  if (sdFontReady && tftPtr != nullptr) {
    return tftPtr->textWidth(text);
  }
  if (u8g2Ptr != nullptr && fallbackFontPtr != nullptr) {
    u8g2Ptr->setFont(fallbackFontPtr);
    return u8g2Ptr->getUTF8Width(text);
  }
  return 0;
}

int fontUtf8WrapIndex(const String &text, int maxWidth) {
  if (text.isEmpty()) {
    return 0;
  }

  // 逐字累加寬度，避免每次 substring 全字串量寬（O(n²)）
  int cut = 0;
  int pos = 0;
  int width = 0;
  while (pos < (int)text.length()) {
    const int next = nextUtf8Index(text, pos);
    char ch[8];
    const int len = next - pos;
    if (len <= 0 || len >= (int)sizeof(ch)) {
      break;
    }
    memcpy(ch, text.c_str() + pos, len);
    ch[len] = '\0';
    const int cw = fontTextWidth(ch);
    if (cut > 0 && width + cw > maxWidth) {
      break;
    }
    width += cw;
    cut = next;
    pos = next;
    if (cut > 0 && width >= maxWidth) {
      break;
    }
  }

  if (cut == 0) {
    cut = nextUtf8Index(text, 0);
  }
  return cut;
}

void fontDrawText(TFT_eSPI &tft, int x, int y, const char *text, uint16_t color,
                  uint16_t bg) {
  if (text == nullptr) {
    return;
  }

  if (sdFontReady) {
    tft.setTextColor(color, bg);
    tft.setCursor(x, y);
    tft.print(text);
    return;
  }

  if (u8g2Ptr != nullptr && fallbackFontPtr != nullptr) {
    u8g2Ptr->setFont(fallbackFontPtr);
    u8g2Ptr->setForegroundColor(color);
    u8g2Ptr->setBackgroundColor(bg);
    const int baseline = y + u8g2Ptr->getFontAscent();
    u8g2Ptr->drawUTF8(x, baseline, text);
  }
}

void fontDrawTextRight(TFT_eSPI &tft, int x, int y, const char *text, uint16_t color,
                       uint16_t bg) {
  if (text == nullptr) {
    return;
  }
  const int drawX = x - fontTextWidth(text);
  fontDrawText(tft, drawX, y, text, color, bg);
}

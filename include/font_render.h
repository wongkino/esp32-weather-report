#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <U8g2_for_TFT_eSPI.h>

enum FontRole : uint8_t {
  FONT_ROLE_DETAIL = 0,  // 6pt 預報詳情
  FONT_ROLE_SMALL = 1,   // 10pt meta
  FONT_ROLE_MAIN = 2,    // 12pt 內文
  FONT_ROLE_LARGE = 3,   // 18pt 溫度
};

bool initFontSystem(TFT_eSPI &tft, U8g2_for_TFT_eSPI &u8g2, const uint8_t *fallbackFont);
bool usingSdFont();
bool usingSdDetailFont();
bool usingSdSmallFont();
bool usingSdLargeFont();
void fontUseDetail();
void fontUseSmall();
void fontUseMain();
void fontUseLarge();
int fontLineHeight();
int fontTextWidth(const char *text);
int fontUtf8WrapIndex(const String &text, int maxWidth);
int fontUtf8WrapIndex(const String &text, int startIndex, int maxWidth);
void fontDrawText(TFT_eSPI &tft, int x, int y, const char *text, uint16_t color,
                  uint16_t bg = TFT_BLACK);
void fontDrawTextRight(TFT_eSPI &tft, int x, int y, const char *text, uint16_t color,
                       uint16_t bg = TFT_BLACK);

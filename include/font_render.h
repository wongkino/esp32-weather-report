#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <U8g2_for_TFT_eSPI.h>

bool initFontSystem(TFT_eSPI &tft, U8g2_for_TFT_eSPI &u8g2, const uint8_t *fallbackFont);
bool usingSdFont();
int fontLineHeight();
int fontTextWidth(const char *text);
int fontUtf8WrapIndex(const String &text, int maxWidth);
void fontDrawText(TFT_eSPI &tft, int x, int y, const char *text, uint16_t color,
                  uint16_t bg = TFT_BLACK);
void fontDrawTextRight(TFT_eSPI &tft, int x, int y, const char *text, uint16_t color,
                       uint16_t bg = TFT_BLACK);

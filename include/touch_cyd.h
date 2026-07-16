#pragma once

#include <Arduino.h>

bool touchBegin();

// 回傳螢幕座標（與 TFT rotation 0、240x320 對齊）
bool touchReadScreen(int16_t &x, int16_t &y);

void touchWaitRelease();

// 校準：依序點 左上、右上、右下、左下、中央（螢幕目標座標固定）
static const int TOUCH_CAL_POINTS = 5;
void touchCalClear();
void touchCalStart();                                  // 開始／重設收集
int touchCalStep();                                    // 0..4 進行中，5=完成
bool touchCalAddSample();                              // 讀取當下觸控作為下一步
const char *touchCalPrompt();                          // 目前提示文字
void touchCalTarget(int step, int16_t &tx, int16_t &ty);  // 目標螢幕座標
String touchCalStatusJson();
void touchCalCancel();  // 取消進行中的校準

#pragma once

// CYD microSD 腳位（VSPI）
#define SD_SCK 18
#define SD_MISO 19
#define SD_MOSI 23
#define SD_CS 5

// 四層字型（Processing 一次產出 PFTC6 / 10 / 12 / 18）
// 詳：預報內文／頁尾；小：狀態說明；中：指標／警告；大：溫度
#define SD_FONT_DETAIL_FILE "/PFTC6.vlw"
#define SD_FONT_DETAIL_NAME "PFTC6"

#define SD_FONT_SMALL_FILE "/PFTC10.vlw"
#define SD_FONT_SMALL_NAME "PFTC10"

#define SD_FONT_FILE "/PFTC12.vlw"
#define SD_FONT_NAME "PFTC12"

#define SD_FONT_LARGE_FILE "/PFTC18.vlw"
#define SD_FONT_LARGE_NAME "PFTC18"

// VLW smooth fonts load glyphs into RAM; keep file small (HKO weather subset).
// 超過約 1MB 易造成 Guru Meditation；同一時間只載入一顆
#define SD_FONT_MAX_BYTES (1024 * 1024)

// Mac 製作：
// 1. python3 tools/generate_hko_font_codes.py
// 2. Processing Run Create_font.pde（6=擴充常用字，10/12=精簡，18=僅溫度數字）
// 3. 複製 PFTC6/10/12/18.vlw 到 microSD 根目錄

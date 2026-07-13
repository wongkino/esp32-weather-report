#pragma once

// CYD microSD 腳位（VSPI）
#define SD_SCK 18
#define SD_MISO 19
#define SD_MOSI 23
#define SD_CS 5

// SD 卡根目錄放置：PFTC12.vlw（由 Mac 蘋方-繁 PingFang TC 12pt 轉換）
#define SD_FONT_FILE "/PFTC12.vlw"
#define SD_FONT_NAME "PFTC12"

// VLW smooth fonts load glyphs into RAM; keep file small (HKO weather subset).
#define SD_FONT_MAX_BYTES (4 * 1024 * 1024)

// Mac 製作 VLW 步驟：
// 1. python3 tools/generate_hko_font_codes.py   # 更新天文台 + 常用字
//    可編輯 tools/common_traditional_chars.txt 新增字元
// 2. Cmd+R Run（已預設 PingFang TC、12pt、PFTC12.vlw）
// 3. 確認 PFTC12.vlw 約 150KB~800KB（勿用 19MB 全字庫）
// 4. 複製到 microSD 卡根目錄，插入 CYD

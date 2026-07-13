# esp32 天氣報告 — Agent 開發指引

本文件供 AI Agent（如 Cursor）在此專案中協作時參考。

## 專案概述

- **名稱**：esp32 天氣報告
- **硬體**：ESP32-2432S028（CYD），ILI9341 240×320 TFT，microSD（VSPI）
- **框架**：PlatformIO + Arduino，`default_envs = cyd`
- **語言**：韌體 C++；文件與 UI 文字使用繁體中文

## 核心檔案

| 檔案 | 職責 |
|------|------|
| `src/main.cpp` | Wi-Fi 連線、HKO API、橫屏雙欄 UI、智慧刷新 |
| `src/font_render.cpp` | SD 字型載入、UTF-8 換行、文字繪製 |
| `include/weather_config.h` | API 網址、刷新間隔、分區設定、字型選擇 |
| `include/font_config.h` | SD 腳位、`PFTC12.vlw`、字型大小上限 |
| `include/wifi_config.h` | Wi-Fi 憑證（**勿提交**，已在 `.gitignore`） |
| `include/wifi_config.example.h` | Wi-Fi 範本，供新環境複製 |

## UI 行為（勿隨意改動除非使用者要求）

- 橫屏 `TFT_ROTATION = 3`（320×240）
- 左欄：溫度（黃）、分區名、濕度、雨量、UV
- 右欄：「天氣預報」+ 自動換行，8 秒翻頁
- 底部：僅在有警告時顯示紅底警告列（`warnsum` 的 `name`，非長文勸喻）
- **無**標題列、天氣圖示、Wi-Fi 狀態、更新時間
- 開機會先顯示藍屏，等 Wi-Fi + API 完成後才繪製（已知行為）

## 天氣資料

- 分區溫度／雨量：`rhrread`，測站名須與 `WEATHER_TEMP_STATION` 一致
- 濕度、UV：全港資料（API 無分區欄位）
- 預報：`flw` 的 `forecastDesc`（全港文字，無分區預報 API）
- 警告：`warnsum` 各項的 `name` 欄位
- 刷新：`WEATHER_REFRESH_MS`（30 秒），`weatherDisplayChanged()` 有變才重繪

## 字型注意事項

- SD 卡格式須為 **FAT32**
- 使用精簡字元集（`tools/generate_hko_font_codes.py` + `common_traditional_chars.txt`）
- `PFTC12.vlw` 約 300–500KB；**禁止**載入 19MB 全字庫（Guru Meditation Error）
- 新增 UI 用字時：更新 `common_traditional_chars.txt` → 重跑 Python + Processing → 更新 SD 卡字型

## PlatformIO

```bash
pio run -e cyd -t upload      # 一般 CYD（ILI9341）
pio run -e cyd_st7789 -t upload  # 花屏時改用 ST7789
pio device monitor -b 115200
```

- `board_build.partitions = huge_app.csv`（需要較大 Flash 容納字型相關程式）
- 勿在 `platformio.ini` 提交本機專用 `upload_port`

## 修改原則

1. **最小 diff**：只改與任務相關的程式，不順手重構
2. **沿用慣例**：命名、結構與現有 `main.cpp` / `font_render.cpp` 一致
3. **秘密不入庫**：`wifi_config.h` 含密碼，永遠不 commit
4. **不過度抽象**：避免為一兩行邏輯新增 helper
5. **回應語言**：與使用者溝通使用繁體中文

## 常見任務

### 更換分區

修改 `include/weather_config.h` 的三個 `WEATHER_*` 常數；確認 `rhrread` API 有對應測站名。

### 調整 UI

改 `drawWeatherLeftPanel`、`drawWeatherForecast`、`drawWeatherWarning`；注意 `calcWeatherLayout()` 與警告列高度。

### 新增字元

1. 編輯 `tools/common_traditional_chars.txt`
2. `python3 tools/generate_hko_font_codes.py`
3. Processing Run `tools/Create_font/Create_font.pde`
4. 複製新 `PFTC12.vlw` 到 SD 卡

### 除錯

- Serial 115200：`[Font]`、`[Wi-Fi]`、`[Weather]` 日誌
- 字型載入成功：`[Font] Loaded SD font /PFTC12.vlw`
- HTTP 失敗：檢查 Wi-Fi 與 `data.weather.gov.hk` HTTPS

## 已知限制

- 開機約 5 秒藍屏（等連線與 API）
- `flw` 預報為全港文字，非分區預報
- 濕度為全港參考值，非分區濕度

# esp32 天氣報告 — Agent 開發指引

本文件供 AI Agent（如 Cursor）在此專案中協作時參考。使用者入門與硬體說明見 [README.md](README.md)。

## 專案概述

- **名稱**：esp32 天氣報告
- **硬體**：ESP32-2432S028（CYD），ILI9341 240×320 TFT，microSD（VSPI）
- **框架**：PlatformIO + Arduino，`default_envs = cyd`
- **語言**：韌體 C++；文件與 UI 文字使用繁體中文

## 核心檔案

| 檔案 | 職責 |
|------|------|
| `src/main.cpp` | Wi-Fi、HKO API、直屏單欄 UI、智慧刷新 |
| `src/districts.cpp` | 18 區標籤與測站名（單一定義） |
| `src/font_render.cpp` | SD 四級字型、UTF-8 換行、文字繪製 |
| `src/wifi_portal.cpp` | SoftAP／STA 網頁：Wi-Fi、地區、觸控校準 |
| `src/touch_cyd.cpp` | XPT2046 SoftSPI＋五點仿射校準 |
| `include/weather_config.h` | API、刷新間隔 |
| `include/portal_html.h` | SoftAP／STA 設定頁 HTML |
| `include/weather_warnings.h` | warnsum code → 畫面簡稱 |
| `include/districts.h` | 分區宣告、`clampDistrictIndex` |
| `include/font_config.h` | SD 腳位、`PFTC6`／`10`／`12`／`18` |
| `include/wifi_config.h` | Wi-Fi 逾時／重試常數（無帳密） |

## UI 行為（勿隨意改動除非使用者要求）

- 直屏 `TFT_ROTATION = 0`（240×320），**單欄垂直**黑底白字；四級字型 PFTC6／10／12／18
- 頂部：有警告時紅底白字置中顯示（簡稱）
- 上方：大溫度（18）、低／高溫、濕度（12）
- 中段：天氣預報詳情（6）；單頁顯示，超出以「…」截斷（**不**自動翻頁）
- 下方：裝置 IP｜更新時間（6）
- **無**天氣圖示、雨量、UV、降雨概率、日期／分區名列
- 觸控：僅網頁啟動校準／驗證時使用；天氣畫面不輪詢觸控
- 已連線後用畫面 IP 開網頁：改地區／觸控校準

## 天氣資料

- 分區溫度：`rhrread`，測站名見 `districts.h`（`WEATHER_DISTRICTS[].tempStation`）
- 濕度：全港參考（天文台／京士柏）
- 預報：`flw` 的 `forecastDesc`（全港文字）
- 今日高低溫：`fnd` 第一天
- 警告：`warnsum` 以 `code` 轉簡稱
- 刷新：`WEATHER_REFRESH_MS`（30 秒）；`weatherDisplayChanged()` 有變才整屏重繪，僅 `updateLabel` 變更時 `drawFooterOnly()`

## 字型注意事項

- SD 卡格式須為 **FAT32**
- 四層：`PFTC6`（詳／頁尾）／`PFTC10`（狀態說明）／`PFTC12`（內文）／`PFTC18`（溫度）
- `fontUse*()` 同角色已載入則不重載 SD；行高開機快取
- **禁止**對 12／18 載入近全字庫；同一時間只載入一顆 VLW
- 新增 UI 用字：更新字表 → `generate_hko_font_codes.py` → Processing → 更新 SD

## PlatformIO

```bash
pio run -e cyd -t upload
pio run -e cyd_st7789 -t upload  # 花屏時改用 ST7789
pio device monitor -b 115200
```

- `board_build.partitions = huge_app.csv`
- 勿在 `platformio.ini` 提交本機專用 `upload_port`

## 修改原則

1. **最小 diff**：只改與任務相關的程式
2. **沿用慣例**：命名、結構與現有檔案一致
3. **秘密不入庫**：Wi-Fi 帳密只存 NVS
4. **不過度抽象**
5. **回應語言**：繁體中文

## 常見任務

### 更換分區

執行時用網頁選區，或改 `src/districts.cpp` 的測站對應表。

### 調整 UI

改 `drawHeaderAndTemp`、`drawForecastBlock`、`drawWeatherWarning`；注意 `calcWeatherLayout()`。

### 新增字元

1. 編輯 `tools/common_traditional_chars.txt`（韌體 `src/`、`include/` 內 UI 字串會由腳本自動掃描）
2. `python3 tools/generate_hko_font_codes.py`（離線：`--offline`）
3. Processing Run `tools/Create_font/Create_font.pde`
4. 複製 `PFTC6/10/12/18.vlw` 到 SD 卡根目錄

### 除錯

- Serial 115200：`[Font]`、`[Wi-Fi]`、`[Weather]`、`[Touch]`
- HTTP 失敗：檢查 Wi-Fi 與 `data.weather.gov.hk` HTTPS

## 已知限制

- 開機需等連線與 API
- `flw` 預報與 `fnd` 高低溫為全港，非分區
- 濕度為全港參考值

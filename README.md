# esp32 天氣報告

ESP32-2432S028（Cheap Yellow Display）橫屏天氣顯示板，從香港天文台開放數據 API 取得即時天氣與預報，以繁體中文顯示。

## 功能

- 分區即時天氣（預設：深水埗區）
- 溫度、濕度、今日雨量、紫外線指數
- 天氣預報文字（自動換行、多頁每 8 秒翻頁）
- 生效中的天氣警告（只顯示警告名稱，如「酷熱天氣警告」）
- SD 卡載入蘋方繁體字型（`PFTC12.vlw`）
- 每 30 秒更新，資料無變動時不重繪螢幕
- 背景靜默連線 Wi-Fi，無連線狀態畫面

## 硬體

| 項目 | 說明 |
|------|------|
| 開發板 | ESP32-2432S028（CYD）2.8" TFT |
| 螢幕 | ILI9341，橫屏 320×240 |
| microSD | FAT32，根目錄放 `PFTC12.vlw` |

## 畫面配置

```
┌──────────────┬──────────────────────────────┐
│  33°C        │  天氣預報              1/2  │
│  深水埗區    │  預報文字（自動換行）         │
│  濕 71%      │                              │
│  雨 0mm      │                              │
│  UV8 甚高    │                              │
├──────────────┴──────────────────────────────┤
│  [警告列：有警告時才顯示]                     │
└─────────────────────────────────────────────┘
```

## 快速開始

### 1. 安裝依賴

- [PlatformIO](https://platformio.org/)（VS Code 擴充或 CLI）
- USB 驅動（CP2102 / CH340）

### 2. Wi-Fi 設定

```bash
cp include/wifi_config.example.h include/wifi_config.h
```

編輯 `include/wifi_config.h`，填入 SSID 與密碼（此檔已加入 `.gitignore`，不會上傳）。

### 3. 字型（SD 卡）

**方式 A：使用專案內建字型檔**

將 `tools/Create_font/FontFiles/PFTC12.vlw` 複製到 microSD 卡根目錄。

**方式 B：自行產生**

```bash
python3 tools/generate_hko_font_codes.py
open -a Processing tools/Create_font/Create_font.pde
# Processing 按 Run，產生 PFTC12.vlw 後複製到 SD 卡
```

> SD 卡須為 **FAT32**。字型檔約 150KB–800KB，勿使用 19MB 全字庫（會導致 ESP32 記憶體不足）。

### 4. 編譯與燒錄

```bash
pio run -e cyd -t upload
pio device monitor -b 115200
```

若畫面花屏，改用 ST7789 驅動：

```bash
pio run -e cyd_st7789 -t upload
```

### 5. 修改分區

編輯 `include/weather_config.h`：

```cpp
#define WEATHER_DISTRICT_LABEL "深水埗區"
#define WEATHER_TEMP_STATION "深水埗"      // 須與天文台 API 測站名一致
#define WEATHER_RAINFALL_PLACE "深水埗"
```

## 專案結構

```
├── src/main.cpp              # 主程式：Wi-Fi、API、UI
├── src/font_render.cpp       # SD 字型載入與文字繪製
├── include/
│   ├── weather_config.h      # 天氣 API、分區、刷新間隔
│   ├── font_config.h         # SD 腳位與字型檔名
│   └── wifi_config.example.h # Wi-Fi 範本
├── tools/
│   ├── generate_hko_font_codes.py
│   ├── common_traditional_chars.txt
│   └── Create_font/          # Processing 字型產生腳本
├── platformio.ini
├── agent.md                  # AI Agent 開發指引
└── README.md
```

## API 資料來源

| dataType | 用途 |
|----------|------|
| `rhrread` | 分區溫度、濕度、雨量、UV |
| `flw` | 天氣預報文字 |
| `warnsum` | 生效中的警告名稱 |

資料來源：[香港天文台開放數據](https://www.hko.gov.hk/tc/abouthko/opendata.htm)

## 授權

MIT License

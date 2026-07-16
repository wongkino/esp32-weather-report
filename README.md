# esp32 天氣報告

ESP32-2432S028（Cheap Yellow Display）直屏天氣顯示板，從香港天文台開放數據 API 取得即時天氣與預報，以繁體中文顯示。

**GitHub：** https://github.com/wongkino/esp32-weather-report

## 文件導覽

- 使用者快速開始：本頁
- 開發者操作手冊：[`docs/development.md`](docs/development.md)
- 架構說明：[`docs/architecture.md`](docs/architecture.md)
- 字型與字表說明：[`docs/font-workflow.md`](docs/font-workflow.md)
- 工具目錄說明：[`tools/README.md`](tools/README.md)
- Agent 協作指引：[`agent.md`](agent.md)

## 功能

- 分區即時溫度（網頁選擇 18 區）
- 溫度、濕度、今日高低溫
- 天氣預報文字（自動換行；超出以「…」截斷）
- 生效中的天氣警告（頂部紅底簡稱，如「黃雨」）
- SD 卡四級蘋方繁體字型（`PFTC6`／`10`／`12`／`18.vlw`）
- 每 30 秒更新；資料無變動不整屏重繪（僅更新時間變更時只重繪頁尾）
- SoftAP／STA 網頁設定 Wi-Fi、地區、觸控校準

## 硬體

| 項目 | 說明 |
|------|------|
| 開發板 | ESP32-2432S028（CYD）2.8" TFT |
| 螢幕 | ILI9341，直屏 240×320 |
| microSD | FAT32，根目錄放 `PFTC6.vlw`、`PFTC10.vlw`、`PFTC12.vlw`、`PFTC18.vlw` |

## 畫面配置

```
┌──────────────────────────┐
│      黃雨 雷暴（可選）      │
├──────────────────────────┤
│  28°C     低25°           │
│           高31°           │
│           濕度 80%        │
│                          │
│  大致多雲，日間短暫…       │
│  ……                       │
│                          │
│  192.168.x.x | 天文台更新 上午10:21 │
└──────────────────────────┘
```

## 快速開始

### 1. 安裝依賴

- [PlatformIO](https://platformio.org/)（VS Code 擴充或 CLI）
- USB 驅動（CP2102 / CH340）

### 2. Wi-Fi 與地區設定

無需在程式內填帳密。開機若尚無已存網路，裝置會開熱點 `esp32-weather`，用手機連上後開啟 `http://192.168.4.1`，選擇 Wi-Fi、密碼與天氣地區。

已連線後，可用畫面底部顯示的 IP 開啟同一設定頁更改地區或觸控校準。

### 3. 字型（SD 卡）

**方式 A：使用專案內建字型檔**

將 `tools/Create_font/FontFiles/PFTC6.vlw`、`PFTC10.vlw`、`PFTC12.vlw`、`PFTC18.vlw` 複製到 microSD 卡根目錄。

**方式 B：自行產生**

```bash
python3 tools/generate_hko_font_codes.py          # 含 HKO API + OpenCC
python3 tools/generate_hko_font_codes.py --offline  # 僅本地字表與韌體掃描
open -a Processing tools/Create_font/Create_font.pde
# Processing 按 Run，產生四級 VLW 後複製到 SD 卡
```

> SD 卡須為 **FAT32**。字型檔約 150KB–800KB，勿使用 19MB 全字庫（會導致 ESP32 記憶體不足）。
>
> 更完整的字型維護流程見 [`docs/font-workflow.md`](docs/font-workflow.md)。

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

執行時用網頁選區，或編輯 `src/districts.cpp` 的測站對應表。

## 專案結構

```
├── docs/
│   ├── development.md        # 開發者操作手冊
│   ├── architecture.md       # 模組分工與資料流說明
│   └── font-workflow.md      # 字型、字表與 VLW 維護流程
├── src/
│   ├── main.cpp              # 主程式：Wi-Fi、API、UI
│   ├── districts.cpp         # 18 區與測站定義
│   ├── font_render.cpp       # SD 字型載入與文字繪製
│   ├── wifi_portal.cpp       # SoftAP／STA 網頁設定
│   └── touch_cyd.cpp         # 觸控與校準
├── include/
│   ├── weather_config.h      # 天氣 API、刷新間隔
│   ├── weather_warnings.h    # warnsum code → 簡稱
│   ├── districts.h           # 18 區宣告
│   ├── portal_html.h         # SoftAP／STA 設定頁
│   ├── font_config.h         # SD 腳位與字型檔名
│   ├── font_render.h
│   ├── wifi_config.h         # Wi-Fi 逾時常數
│   ├── wifi_portal.h
│   └── touch_cyd.h
├── tools/
│   ├── README.md             # 工具目錄與用途說明
│   ├── generate_hko_font_codes.py
│   ├── common_traditional_chars.txt
│   └── Create_font/          # Processing 字型產生腳本
├── platformio.ini
├── agent.md                  # AI Agent 協作與修改約束
└── README.md
```

開發時建議先讀 [`docs/development.md`](docs/development.md)，再依需要查看架構與字型文件。
## API 資料來源

| dataType | 用途 |
|----------|------|
| `rhrread` | 分區溫度、濕度 |
| `flw` | 天氣預報文字 |
| `fnd` | 今日高低溫 |
| `warnsum` | 生效中的警告 |

資料來源：[香港天文台開放數據](https://www.hko.gov.hk/tc/abouthko/opendata.htm)

## 授權

MIT License

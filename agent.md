# esp32 天氣報告 — Agent 開發指引

本文件供 AI Agent（如 Cursor）在此專案中協作時參考。重點是「修改約束」與「工作入口」，而非重複使用者文件。

## 文件入口

- 使用者入門與硬體說明：[`README.md`](README.md)
- 模組分工與資料流：[`docs/architecture.md`](docs/architecture.md)
- 字型與字表流程：[`docs/font-workflow.md`](docs/font-workflow.md)

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
- 新增 UI 用字時，請依 [`docs/font-workflow.md`](docs/font-workflow.md) 更新字表與 VLW 檔

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

請依 [`docs/font-workflow.md`](docs/font-workflow.md) 的流程更新字表與字型檔。

### 除錯

- Serial 115200：`[Font]`、`[Wi-Fi]`、`[Weather]`、`[Touch]`
- HTTP 失敗：檢查 Wi-Fi 與 `data.weather.gov.hk` HTTPS

## 已知限制

- 開機需等連線與 API
- `flw` 預報與 `fnd` 高低溫為全港，非分區
- 濕度為全港參考值

# 架構說明

本文件說明韌體模組分工、資料流與主要設定點，供開發者快速理解專案結構。

## 文件導覽

- 專案總覽與快速開始：[`../README.md`](../README.md)
- 開發者操作手冊：[`development.md`](development.md)
- 字型與字表說明：[`font-workflow.md`](font-workflow.md)
- 多 Agent 協作規範：[`agents.md`](agents.md)

## 模組分工

| 路徑 | 職責 |
|------|------|
| `src/main.cpp` | 主流程：開機、Wi-Fi、天氣 API、畫面刷新、觸控模式切換 |
| `src/districts.cpp` | 18 區標籤與測站對應 |
| `src/font_render.cpp` | SD 字型載入、UTF-8 換行、文字繪製 |
| `src/wifi_portal.cpp` | SoftAP / STA 設定頁、地區切換、觸控校準入口 |
| `src/touch_cyd.cpp` | XPT2046 觸控讀值、校準與座標映射 |
| `include/weather_config.h` | API 端點、刷新間隔、HTTP timeout |
| `include/weather_warnings.h` | `warnsum` code 到畫面簡稱的對照 |
| `include/font_config.h` | SD 腳位與字型檔名設定 |
| `include/wifi_config.h` | Wi-Fi 連線 timeout / retry 常數 |

## 執行流程

1. 開機初始化螢幕、字型與觸控
2. 讀取已儲存的地區與 Wi-Fi 設定
3. 優先嘗試 STA 連線；失敗時開啟 `esp32-weather` 設定熱點
4. 透過香港天文台 API 取得：
   - `rhrread`：分區溫度、濕度
   - `flw`：天氣預報文字
   - `fnd`：今日高低溫
   - `warnsum`：生效中的天氣警告
5. 資料有變動時才重繪整屏；若只有更新時間改變，僅重繪頁尾
6. 已連線時持續提供設定頁，可改地區與觸控校準

## 畫面組成

- 頂部：生效中的警告（有資料時才顯示）
- 上半部：目前溫度、低溫、高溫、濕度
- 中段：預報文字
- 底部：裝置 IP 與天文台更新時間

版面計算集中在 `calcWeatherLayout()`，主要繪圖入口為：

- `drawWeatherScreen()`
- `drawHeaderAndTemp()`
- `drawForecastBlock()`
- `drawWeatherWarning()`
- `drawFooter()`

## 修改建議

### 調整地區資料

- 執行時優先使用網頁設定
- 若要改預設對應，編輯 `src/districts.cpp`

### 調整畫面

- 版面：修改 `calcWeatherLayout()`
- 溫度/濕度：修改 `drawHeaderAndTemp()`
- 預報區：修改 `drawForecastBlock()`
- 警告列：修改 `drawWeatherWarning()`

### 調整天氣抓取

- API URL 組裝：`buildApiUrl()`
- HTTP + JSON 解析：`httpGetJson()`
- 整體抓取流程：`fetchWeather()`

## 已知限制

- 開機需等待 Wi-Fi 與 API 回應
- `flw` 與 `fnd` 為全港資料，非分區
- 濕度使用全港參考站資料
- 目前天氣主畫面不提供一般觸控操作

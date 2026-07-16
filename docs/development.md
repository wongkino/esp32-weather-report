# 開發手冊

本文件整理日常開發、驗證與常見修改流程，作為開發者操作入口。

## 文件導覽

- 專案總覽與快速開始：[`../README.md`](../README.md)
- 模組分工與資料流：[`architecture.md`](architecture.md)
- 字型與字表流程：[`font-workflow.md`](font-workflow.md)
- 工具目錄說明：[`../tools/README.md`](../tools/README.md)

## 開發環境

### 必要工具

- [PlatformIO](https://platformio.org/)（VS Code 擴充或 CLI）
- Python 3（字型字表腳本）
- USB 驅動（CP2102 / CH340，視開發板而定）

### 主要設定

- 預設環境：`cyd`
- 框架：Arduino
- 平台：ESP32

主要設定檔為 [`../platformio.ini`](../platformio.ini)。

## 常用命令

### 編譯

```bash
pio run -e cyd
```

### 燒錄

```bash
pio run -e cyd -t upload
```

### 序列監看

```bash
pio device monitor -b 115200
```

### ST7789 備用驅動

若 ILI9341 畫面花屏，可改用：

```bash
pio run -e cyd_st7789 -t upload
```

## 常見開發任務

### 修改分區與測站

- 執行時優先透過網頁設定切換地區
- 若要調整預設對應，編輯 `src/districts.cpp`

### 修改畫面配置

優先參考 [`architecture.md`](architecture.md) 中的畫面與模組說明。

常見入口：

- `calcWeatherLayout()`：版面計算
- `drawHeaderAndTemp()`：溫度 / 濕度區塊
- `drawForecastBlock()`：預報區塊
- `drawWeatherWarning()`：警告列
- `drawFooter()`：頁尾資訊

### 修改天氣抓取邏輯

- `buildApiUrl()`：API URL 組裝
- `httpGetJson()`：HTTP + JSON 解析
- `fetchWeather()`：整體資料抓取與欄位填充

### 修改 Wi-Fi 設定頁

- 後端流程：`src/wifi_portal.cpp`
- 前端頁面：`include/portal_html.h`

### 修改觸控校準

- 觸控與校準：`src/touch_cyd.cpp`
- 入口流程：`src/main.cpp` 與 `src/wifi_portal.cpp`

## 驗證建議

### 基本驗證

1. 成功開機並顯示狀態畫面
2. 能正常連上 Wi-Fi 或進入設定熱點
3. 能取得天氣資料並顯示
4. 已連線後可開啟設定頁
5. 變更地區後畫面會刷新

### 字型相關驗證

1. SD 卡可正常掛載
2. 6 / 10 / 12 / 18 pt 字型都能載入
3. 預報與警告無缺字

若涉及字型調整，請一併參考 [`font-workflow.md`](font-workflow.md)。

## 除錯重點

使用序列埠 `115200`，常見標記：

- `[Font]`
- `[Wi-Fi]`
- `[Weather]`
- `[Touch]`

### 常見問題

- 無法取天氣：檢查 Wi-Fi 與 `data.weather.gov.hk` HTTPS
- 顯示缺字：重新產生 VLW 字型檔
- 花屏：改測 `cyd_st7789`
- 無法進入主畫面：檢查 Wi-Fi 憑證、API 回應與 SD 卡

## 修改原則

- 優先最小差異修改
- 避免引入不必要抽象
- 保持繁體中文 UI 與文件用語一致
- Wi-Fi 帳密只存 NVS，不入版本庫

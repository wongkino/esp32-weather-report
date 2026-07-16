# 字型與字表說明

本專案使用 SD 卡上的 VLW 字型檔，以降低 ESP32 記憶體壓力並保留繁體中文顯示能力。

## 文件導覽

- 專案總覽與快速開始：[`../README.md`](../README.md)
- 開發者操作手冊：[`development.md`](development.md)
- 架構說明：[`architecture.md`](architecture.md)
- 工具目錄說明：[`../tools/README.md`](../tools/README.md)
- 多 Agent 協作規範：[`agents.md`](agents.md)

## 使用的字型層級

| 字型 | 用途 |
|------|------|
| `PFTC6.vlw` | 預報詳情、頁尾 |
| `PFTC10.vlw` | 狀態說明文字 |
| `PFTC12.vlw` | 一般內文 |
| `PFTC18.vlw` | 大溫度數字 |

## 相關檔案

| 路徑 | 用途 |
|------|------|
| `tools/generate_hko_font_codes.py` | 蒐集 UI/HKO/OpenCC 字元並更新 Processing 字表 |
| `tools/common_traditional_chars.txt` | 主字表補充字元 |
| `tools/common_traditional_chars_detail.txt` | 6pt 詳情字表補充字元 |
| `tools/Create_font/Create_font.pde` | Processing 腳本，輸出 VLW 檔 |
| `include/font_config.h` | 韌體使用的字型檔名與 SD 設定 |

## 產生流程

### 方式 A：使用既有字型檔

將以下檔案放到 microSD 卡根目錄：

- `PFTC6.vlw`
- `PFTC10.vlw`
- `PFTC12.vlw`
- `PFTC18.vlw`

### 方式 B：重新產生字型檔

```bash
python3 tools/generate_hko_font_codes.py
python3 tools/generate_hko_font_codes.py --offline
open -a Processing tools/Create_font/Create_font.pde
```

接著在 Processing 執行 `Create_font.pde`，再把產出的 VLW 檔複製到 SD 卡根目錄。

## 什麼時候需要更新字表

- 新增或修改 UI 文案
- 預報顯示缺字
- 想讓 6pt 詳情字型支援更多繁體字

## 建議流程

1. 修改 `src/` 或 `include/` 中的 UI 字串
2. 視需要補充 `tools/common_traditional_chars.txt`
3. 執行 `generate_hko_font_codes.py`
4. 在 Processing 重新輸出 VLW
5. 更新 SD 卡字型檔
6. 重新燒錄／重啟裝置驗證顯示

## 注意事項

- SD 卡需為 **FAT32**
- 不要將 12pt / 18pt 改成近全字庫，否則容易導致記憶體不足
- `PFTC18` 主要給溫度數字使用，字集應保持精簡
- 同一時間只會載入一顆 VLW 字型，避免額外記憶體占用

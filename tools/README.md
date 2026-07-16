# 工具目錄說明

本目錄放置字型與字表相關的輔助工具，主要用於維護繁體中文字型覆蓋範圍。

## 目錄內容

| 路徑 | 用途 |
|------|------|
| `generate_hko_font_codes.py` | 蒐集韌體 UI、HKO API 與 OpenCC 字元，更新 Processing 字表 |
| `common_traditional_chars.txt` | 主字表補充字元 |
| `common_traditional_chars_detail.txt` | 6pt 詳情字表補充字元 |
| `Create_font/Create_font.pde` | Processing 腳本，輸出 VLW 字型檔 |

## 典型工作流程

1. 修改韌體 UI 字串
2. 視需要補充 `common_traditional_chars.txt`
3. 執行 `generate_hko_font_codes.py`
4. 用 Processing 開啟 `Create_font/Create_font.pde`
5. 產生 VLW 檔並複製到 SD 卡

更完整流程請見 [`../docs/font-workflow.md`](../docs/font-workflow.md)。

## 注意事項

- 本目錄中的腳本與字表檔彼此有流程依賴，不建議任意刪除
- `common_traditional_chars_detail.txt` 供 6pt 詳情字型擴充使用
- `Create_font.pde` 內容部分會由 `generate_hko_font_codes.py` 自動更新

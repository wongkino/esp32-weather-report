# AI 協作指引

本文件是 AI Agent 的**精簡入口**。完整角色分工、工作流與文件更新規則見 [`docs/agents.md`](docs/agents.md)。

## 先讀這些

| 需求 | 文件 |
|------|------|
| 專案總覽 | [`README.md`](README.md) |
| 開發與驗證 | [`docs/development.md`](docs/development.md) |
| 模組與資料流 | [`docs/architecture.md`](docs/architecture.md) |
| 字型與字表 | [`docs/font-workflow.md`](docs/font-workflow.md) |
| 多 Agent 規範 | [`docs/agents.md`](docs/agents.md) |

## 必守約束

- 回應語言：**繁體中文**
- 只改與任務相關的檔案，保持最小 diff
- Wi-Fi 帳密只存 NVS，不入庫
- 直屏 240×320、單欄黑底白字；四級字型 PFTC6／10／12／18
- 預報單頁顯示，超出以「…」截斷；天氣主畫面不輪詢觸控
- 禁止 12／18pt 載入近全字庫；同一時間只載入一顆 VLW
- 未經要求，不新增天氣圖示、雨量、UV、降雨概率等欄位
- 資料無變動不重繪整屏；僅 `updateLabel` 變更時只重繪頁尾

## 快速命令

```bash
pio run -e cyd -t upload
pio run -e cyd_st7789 -t upload   # 花屏時
pio device monitor -b 115200
```

## 文件怎麼改

- 對外功能／快速開始變了 → `README.md`
- 開發流程變了 → `docs/development.md`
- 架構變了 → `docs/architecture.md`
- 字型流程變了 → `docs/font-workflow.md`
- Agent 規則變了 → `docs/agents.md`（本檔僅保留摘要）

細節請依 [`docs/agents.md`](docs/agents.md) 的「文件更新規則」執行。

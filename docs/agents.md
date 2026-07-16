# 多 Agent 協作規範

本文件定義本專案中 AI Agent 的角色分工、工作邊界、交付標準與文件更新規則。  
精簡入口請見 [`../agent.md`](../agent.md)。

## 文件導覽

- AI 協作精簡入口：[`../agent.md`](../agent.md)
- 專案總覽：[`../README.md`](../README.md)
- 開發者操作手冊：[`development.md`](development.md)
- 架構說明：[`architecture.md`](architecture.md)
- 字型與字表說明：[`font-workflow.md`](font-workflow.md)

## 適用範圍

- Cursor Cloud Agent、IDE Agent、背景自動化代理
- 人類開發者委派給 AI 的實作、重構、文件與審查任務

## Agent 角色

| 角色 | 主要職責 | 典型輸出 |
|------|----------|----------|
| **Implementer** | 依需求修改韌體或工具腳本 | 程式碼 diff、簡短變更說明 |
| **Reviewer** | 檢查 diff 是否違反專案約束 | 問題清單、風險分級、建議 |
| **Docs** | 維護 README / docs / tools 說明 | 文件更新、導覽連結同步 |
| **Optimizer** | 效能、記憶體、重繪與字串配置優化 | 局部重構、量測或推論依據 |
| **Verifier** | 編譯、燒錄前檢查、回歸驗證 | 驗證結果、未覆蓋風險 |

單一 Agent 可兼任多角色，但必須在回覆中標明目前扮演的角色。

## 工作原則

1. **最小 diff**：只改與任務直接相關的檔案。
2. **沿用慣例**：命名、模組邊界、UI 行為與現有程式一致。
3. **秘密不入庫**：Wi-Fi 帳密只存 NVS，不得寫入原始碼或文件範例。
4. **不過度抽象**：避免為小改動新增多層 helper。
5. **回應語言**：對使用者一律使用繁體中文。
6. **先讀再改**：動手前確認相關模組與文件入口。

## 不可擅自變更的約束

除非使用者明確要求，否則不得修改：

| 類別 | 約束 |
|------|------|
| 畫面方向 | 直屏 `TFT_ROTATION = 0`（240×320） |
| 版面風格 | 單欄垂直、黑底白字 |
| 字型層級 | PFTC6 / 10 / 12 / 18 四級分工 |
| 預報顯示 | 單頁顯示，超出以「…」截斷，不自動翻頁 |
| 觸控策略 | 天氣主畫面不輪詢觸控；僅校準／驗證模式使用 |
| 功能範圍 | 不新增天氣圖示、雨量、UV、降雨概率、日期／分區名列 |
| 字型資源 | 禁止 12／18pt 載入近全字庫；同一時間只載入一顆 VLW |
| 平台設定 | 不在 `platformio.ini` 提交本機 `upload_port` |

## 角色邊界

### Implementer

- 可改：`src/`、`include/`、`tools/`（與任務相關部分）
- 任務完成後應自我檢查是否需同步 Docs Agent 範圍內的文件
- 不得順手重構無關模組

### Reviewer

- 只讀為主；若發現必修正問題，應列為 blocking / non-blocking
- 重點檢查：UI 約束、記憶體風險、NVS 秘密、字型策略、API 容錯

### Docs

- 優先更新專題文件，而非每次改 README
- 保持文件導覽、標題格式、術語一致
- 不得把 agent 規則重複貼滿 README

### Optimizer

- 優先局部、可驗證的優化（HTTP 串流解析、換行配置、helper 可見性）
- 需說明預期效益與可能的行為風險
- 不得為優化而改變 UI 行為或資料語意

### Verifier

- 優先執行 `pio run -e cyd`
- 若環境缺少 PlatformIO，必須明確標註「未驗證」
- 驗證失敗時回報錯誤與可能根因，不假裝通過

## 標準工作流程

```text
1. 讀取 agent.md → 確認任務屬於哪個角色
2. 讀取對應 docs（development / architecture / font-workflow）
3. 實作或審查
4. 視需要執行編譯驗證
5. 更新應改的文件（見下方文件更新規則）
6. 提交並在 PR / 回覆中說明變更範圍與未驗證項
```

## 文件更新規則

| 變更類型 | 應更新文件 |
|----------|------------|
| 使用者可見功能、安裝、快速開始 | `README.md` |
| 開發流程、命令、驗證方式 | `docs/development.md` |
| 模組分工、資料流、主要函式入口 | `docs/architecture.md` |
| 字型、字表、VLW 產生流程 | `docs/font-workflow.md` |
| 工具腳本用途或目錄結構 | `tools/README.md` |
| Agent 角色、邊界、協作規則 | `docs/agents.md` |
| Agent 精簡入口或必守約束摘要 | `agent.md` |

**原則：README 保持穩定入口；細節優先寫入 `docs/`。**

## 常見任務對照

| 任務 | 主要角色 | 優先閱讀 | 主要修改點 |
|------|----------|----------|------------|
| 調整 UI 文案或版面 | Implementer | `architecture.md` | `src/main.cpp` |
| 調整天氣 API 邏輯 | Implementer | `architecture.md` | `src/main.cpp` |
| 調整 Wi-Fi 設定頁 | Implementer | `development.md` | `wifi_portal.cpp`, `portal_html.h` |
| 新增 UI 用字 | Implementer + Docs | `font-workflow.md` | `tools/`, SD 字型 |
| 記憶體或重繪優化 | Optimizer | `architecture.md` | 相關 `src/` |
| 文件架構整理 | Docs | `README.md`, `docs/` | 文件導覽與術語 |
| PR 審查 | Reviewer | `agent.md`, `agents.md` | 評論，不直接擴 scope |

## 交付格式

Agent 完成任務時，回覆應包含：

1. **做了什麼**：一句話摘要
2. **改了哪些檔案**：路徑列表
3. **為什麼這樣改**：與任務的對應關係
4. **驗證結果**：已執行或未執行的命令與原因
5. **未涵蓋風險**：例如無法實機測試、未跑 PlatformIO

## 除錯與日誌

- Serial：`115200`
- 常見標記：`[Font]`、`[Wi-Fi]`、`[Weather]`、`[Touch]`
- API 問題：檢查 Wi-Fi 與 `data.weather.gov.hk` HTTPS

## 已知限制

- 開機需等待 Wi-Fi 與 API
- `flw` 預報與 `fnd` 高低溫為全港資料，非分區
- 濕度為全港參考值

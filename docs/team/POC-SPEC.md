# PoC-001：Harbour + WebUI 最小可行原型

- 狀態：**已核准**（使用者於 2026-11-26 核准，實作位於 `poc/harbour-webui/`）
- 負責人：PM（規格）、Architect（技術做法）、Back-end（實作）、QA（驗收）
- 相關文件：[CHARTER.md](CHARTER.md)、[ENVIRONMENT.md](ENVIRONMENT.md)

## 1. 目標

驗證以下三件事可以在使用者的 Windows 環境中一起運作：

1. Harbour 程式（`.prg`）能作為後端，處理 WebUI 傳來的請求。
2. WebUI 能顯示 HTML 介面，並能從網頁呼叫 Harbour 函式、取得回傳值。
3. 團隊自寫的 C 膠合層（bridge）能以標準 C89 通過編譯。

## 2. 範圍

- 一個視窗，顯示一個輸入框與一個按鈕。
- 按鈕點擊後，網頁呼叫 Harbour 函式 `Hello(cName)`。
- Harbour 回傳字串（例如 `Harbour says: Hello, <name>！第 N 次呼叫`，其中 N 由 Harbour 端計數），網頁顯示回傳結果。

## 3. 非範圍

- MySQL 或任何資料庫存取。
- FiveWin 相關程式。
- WebApp、TLS、遠端存取。
- 效能與長時間穩定性測試。

## 4. 架構

```
[瀏覽器視窗 (WebUI 提供)]
        │  WebSocket（WebUI 內建）
[WebUI 靜態函式庫 webui-2-static.lib]   ← 第三方，不符合 C89，不得被團隊程式直接 include
        │
[bridge：團隊 C89 膠合層]               ← 自行宣告所需的 WebUI API，符合 C89
        │  hb_dynsymFind / hb_vmDo
[Harbour 程式 (.prg)]                   ← Hello() 函式
```

要點：

- **隔離層**：bridge 不引用 `webui.h`，而是自行宣告用到的函式原型（只用指標與基本型別，不使用 `bool`）。WebUI 的 `webui.h` 含有 `//` 註解與 C99 語法，無法通過 `-std=c89 -pedantic-errors`。
- **Harbour 標頭檔同樣隔離**（實作時新增）：`hbdefs.h` 使用 `long long`，`hbapi.h` 因此也無法通過 `-pedantic-errors`。bridge 自行宣告所需的 Harbour API 子集（`hb_parni`、`hb_vmDo` 等皆為真實匯出函式），並以 `HB_FUN_<NAME>` 定義 `HB_FUNC`。不採用 `HB_LONG_LONG_OFF`，因為會改變 `HB_MAXINT` 而破壞 ABI。
- **回傳值**：WebUI 的 `bool` 回傳值不在 bridge 中讀取，以避免 ABI 型別不一致。
- **呼叫 Harbour**：WebUI 的事件回呼在 WebUI 的執行緒中執行，回呼內以 `hb_dynsymFind`、`hb_vmPushDynSym`、`hb_vmPushNil`、`hb_vmDo` 呼叫 Harbour 函式。Harbour VM 只能在單一執行緒中使用，因此 PoC 的回呼必須序列執行，主執行緒在 `webui_wait()` 中閒置。
- **編譯參數**：bridge 與團隊程式以 `-std=c89 -pedantic-errors`（GCC）或 BCC / MSVC 等效設定編譯。

## 5. 交付物

放置於 `poc/harbour-webui/`：

- `src/bridge.c`：C89 膠合層，提供 Harbour 可呼叫的 `HB_FUNC` 包裝與 WebUI 回呼。
- `prg/main.prg`：Harbour 主程式，建立視窗、綁定 `Hello()`。
- `prg/hello.prg`：`Hello(cName)` 函式與呼叫計數。
- `ui/index.html`：輸入框、按鈕與結果顯示區。
- 建置說明：`poc/harbour-webui/README.md`，列出 bcc_x32 與 vs2019-x32 的建置步驟。

## 6. 驗收標準

| # | 項目 | 通過條件 |
|---|---|---|
| A1 | bridge C89 合規 | `bridge.c` 以 `-std=c89 -pedantic-errors` 編譯無錯誤（GCC 驗證；BCC 與 MSVC 於對應環境驗證） |
| A2 | 編譯與連結 | bcc_x32 完成 WebUI 靜態函式庫、Harbour 程式與 bridge 的建置與連結 |
| A3 | 執行與互動 | 程式啟動後視窗顯示介面，輸入名稱並點擊按鈕，結果正確顯示 |
| A4 | 呼叫計數 | 連續點擊三次，回傳的計數依序為 1、2、3 |
| A5 | 退場 | 關閉視窗後程式正常結束 |

A2 備註（修訂 1）：vs2019-x32 為靜態庫 webui-2-static.lib；bcc_x32 為 webui-2.dll 加 BCC 匯入函式庫（implib 產生）。

- 若 bcc_x32 無法通過 A2 或 A3，改以 vs2019-x32 驗證，並於 README 記錄失敗原因。
- vs2019-x64 於 PoC 通過後再驗證。

## 7. 風險

| 風險 | 影響 | 對策 |
|---|---|---|
| WebUI 原始碼在 BCC 7.4 下無法編譯（`//` 註解、C99 語法） | A2 無法通過 | 測試後若失敗，改用 vs2019-x32；並評估對 WebUI 原始碼做最小化修改，或改用 BCC 的 C++ 模式編譯 WebUI |
| WebUI 回呼執行緒與 Harbour VM 不相容 | A3 結果異常或當機 | 回呼序列執行，主執行緒閒置；若仍有問題，改為由主執行緒輪詢佇列 |
| 第三方函式庫 ABI 與宣告不一致 | 執行時崩潰 | bridge 只使用指標與 `int` 型別參數，宣告與 `webui.h` 逐一對照 |

## 8. 核准

- [x] 使用者核准本規格（2026-11-26），實作開始。
- 驗收結果見 [poc/harbour-webui/README.md](../../poc/harbour-webui/README.md)。

## 9. 修訂紀錄

| 修訂 | 日期 | 內容 | 狀態 |
|---|---|---|---|
| 1 | 2026-11-26 | bcc_x32 路線改為 WebUI DLL 加 BCC 匯入函式庫（選項 D，主要路線）；vs2019-x32 維持靜態庫；A1 至 A5 不變；新增交付物 webui-2.dll 與匯入庫（使用者產生）；執行時 DLL 須與 hbwebui.exe 同資料夾；WebUI 須為 2.5.0-beta.2 或更新，且 bridge 與 DLL 同版本 | 使用者核准（選項 A），2026-11-26 |

BCC 原始碼編譯降為備援。

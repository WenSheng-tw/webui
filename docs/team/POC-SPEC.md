# PoC-001：Harbour + WebUI 最小可行原型

- 狀態：**待使用者核准**
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

- 若 bcc_x32 無法通過 A2 或 A3，改以 vs2019-x32 驗證，並於 README 記錄失敗原因。
- vs2019-x64 於 PoC 通過後再驗證。

## 7. 風險

| 風險 | 影響 | 對策 |
|---|---|---|
| WebUI 原始碼在 BCC 7.4 下無法編譯（`//` 註解、C99 語法） | A2 無法通過 | 測試後若失敗，改用 vs2019-x32；並評估對 WebUI 原始碼做最小化修改，或改用 BCC 的 C++ 模式編譯 WebUI |
| WebUI 回呼執行緒與 Harbour VM 不相容 | A3 結果異常或當機 | 回呼序列執行，主執行緒閒置；若仍有問題，改為由主執行緒輪詢佇列 |
| 第三方函式庫 ABI 與宣告不一致 | 執行時崩潰 | bridge 只使用指標與 `int` 型別參數，宣告與 `webui.h` 逐一對照 |

## 8. 核准

- [ ] 使用者核准本規格後，才開始實作。

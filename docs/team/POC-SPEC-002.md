# PoC-002：WebUI 完整 API 的 C89 綁定與記憶體回收

- 狀態：**已核准**（使用者於 2026-11-26 核准：選項 A，範圍內函式完整綁定、分波交付，每波經 QA 通過後才進下一波；雙模式測試已核准；Windows 延後，以 Linux 為準）
- 負責人：PM（規格）、Architect（做法）、Back-end（實作）、QA（驗收）
- 相關文件：[CHARTER.md](CHARTER.md)、[POC-SPEC.md](POC-SPEC.md)（PoC-001，A1 至 A5 不變）、[ENVIRONMENT.md](ENVIRONMENT.md)

## 1. 範圍

`include/webui.h` 共 112 個公開函式。分組如下，數量為估計值，以 W0 產出的函式清單為準。

| 分組 | 內容 |
|---|---|
| 視窗（約 25） | new_window、destroy、close、minimize、maximize、size、position、kiosk、icon、focus 等 |
| 綁定與腳本（約 12） | bind、run、script、`interface_bind` |
| 顯示與瀏覽器（約 10） | show 系列、start_server、navigate、get_url、browser_exist |
| 等待與結束（約 7） | wait、wait_async、exit、clean、timeout |
| 字串與回傳（約 30） | get_*_at、return_*，以及 `interface_*` 對應版 |
| 設定（約 12） | set_config（列舉以 int 傳入）、runtime、profile、proxy、TLS、last_error |
| 網路與連接埠（約 4） | get_port、set_port、get_free_port |
| 資料夾（約 3） | root_folder、default_root_folder、browser_folder |
| 其他 | encode、decode、free、get_mime_type 等 |

**排除（共 12 個）：**

| 函式 | 原因 |
|---|---|
| `webui_set_context`、`webui_get_context`、`webui_malloc`、`webui_memcpy`、`webui_get_hwnd`、`webui_win32_get_hwnd` | 回傳或接收原始指標，Harbour 無法安全持有 |
| `webui_set_logger`、`webui_set_close_handler_wv`、`webui_set_file_handler`、`webui_set_file_handler_window` | 需要把回呼傳到 Harbour，並回傳不屬於 Harbour 的記憶體 |
| `webui_return_http`、`webui_interface_set_response_file_handler` | 同上 |

範圍內共 100 個函式。

**取代而非排除：** 取用 `webui_event_t*` 的函式，改用 `interface_*` 版（以視窗與事件編號為參數）。Harbour 端不接觸任何指標。

## 2. 架構

- 檔案：`bridge_decl.h`（API 宣告）、`bridge_core.c`、`bridge_win.c`、`bridge_event.c`、`bridge_misc.c`。
- 每個函式一個 `HB_FUN_WEBUI_*`。
- 視窗與 ID 都用 Harbour 數字。
- 64 位元整數改用 `double`（`hb_parnd`、`hb_retnd`），精度上限 2^53。
- 處理函式從 PoC-001 的單一處理函式改為**表格**，以 bind id 為索引。保留舊的 `WEBUI_BINDHB` 簽名。

## 3. 記憶體回收

- **必須釋放：** 只有 `webui_encode` 與 `webui_decode` 回傳堆積記憶體（原始碼已確認）。用 `hb_retc` 複製後，立即呼叫 **`webui_free`** 釋放。不使用 `hb_retc_buffer`，因為它會用 `hb_xfree`，與 WebUI 的配置器不同。
- **不得釋放：** `get_string_at`、`get_url`、`start_server`、`get_mime_type`、`get_last_error_message` 回傳的字串屬 WebUI，只能用 `hb_retc` 當場複製。
- **bridge 暫存：** 例如 `webui_script` 的緩衝區，用 `hb_xgrab` 取得，複製後 `hb_xfree`。輸入字串只在呼叫期間有效。若 WebUI 會保留指標（例如 `set_custom_parameters`），bridge 先複製並登記，再次設定或清理時釋放。Architect 須逐一稽核 `webui.c` 中的保留情形。
- **結束：** Harbour 在 `WEBUI_WAIT` 返回後呼叫 `WEBUI_CLEAN`，它呼叫 `webui_clean` 並釋放處理函式表。`webui_clean` 若在回呼內呼叫，只會要求結束，因此必須在 wait 之後呼叫（原始碼已確認）。
- **證明：**
  - valgrind 的 `--leak-check=full`。
  - 除錯版的配置與釋放計數器，結束後須為 0。

## 4. C89 規則

- 不使用 `//`；宣告放在區塊開頭；不使用 `long long`；不 include 任何 C99 標頭。
- `webui.h` 與 `hbapi.h` 仍不 include，與 PoC-001 一致，因為兩者含 C99 語法。bridge 自行宣告所用子集。
- `bool` 回傳宣告為 `unsigned char`。參數以 `int` 傳入。
- 另做一次 C99 對照編譯，確認宣告與真實標頭沒有衝突。

## 5. 驗收

| # | 通過條件 |
|---|---|
| A6 | 每個範圍內函式可由 Harbour 呼叫，並有對應測試 |
| A7 | 所有 bridge 檔案通過 `gcc -std=c89 -pedantic-errors` |
| A8 | 腳本化執行下 valgrind 的 definite leak 為 0，且計數器為 0 |
| A9 | 以 Linux 為準；Windows 依使用者決定延後 |
| A10 | PoC-001 的 A1 至 A5 仍通過 |
| A11 | 模式 1 內建通過全部函式測試 |
| A12 | 模式 2 以 `hb_hrbLoad` 通過同一批測試 |
| A13 | 雜湊規則四項皆有測試：無 hrb 則編譯；雜湊相同則不編譯；`.prg` 變更則重編；缺 `.prg` 則用 hrb |
| A14 | 同一份測試原始碼在 Linux 兩模式的結果相同 |

## 6. 風險

1. **阻擋：** `bool` 的 ABI（只能用 `unsigned char` 讀取）與 `double` 精度。對策：先做 ABI 測試。
2. **主要：** 回呼在 WebUI 執行緒呼叫 Harbour VM，以及 `webui_script` 在回呼內的阻塞與死鎖。
3. **主要：** WebUI 保留字串指標的函式，生命週期未文件化。
4. **主要：** 需要視窗的函式要在 Xvfb 加瀏覽器下測試。
5. **主要：** 未被 REQUEST 的函式在 `.hrb` 載入時失敗。對策是 A12 的解析檢查。
6. **主要：** exe 體積增加，Windows 上需確認 `hbcplr` 等函式庫存在於 BCC 與 vs2019 的 Harbour 建置（Windows 延後，屆時驗證）。
7. **次要：** WebUI 內部的洩漏不屬於我們，須在報告中分開記載。
8. **次要：** `.hrb` 未卸載會干擾 valgrind 的洩漏判斷，需 `hb_hrbUnload`。
9. **次要：** `.ch` 不在雜湊內。

工作量（技術描述）：新增 5 個 C 檔案、約 100 個包裝函式、每組一支測試 `.prg`，另有 `bridge_request.prg`、雜湊與載入輔助、測試執行器與兩個建置腳本。需稽核約 20 個保留字串的函式。不動 `webui.c`，只改 `bridge.c` 的結構與 `main.prg`。

## 7. 分波計畫

每波由 Back-end 實作、QA 驗證，通過後才進下一波。函式數為估計值，待 W0 函式清單定案。

| 波 | 內容 | 負責 | 函式數 | 必要測試 | 出口標準 |
|---|---|---|---|---|---|
| W0 | 前置：字串保留稽核、`bool` 與 64 位元 ABI 測試、`bridge_request.prg` 產生器與函式清單 | Architect、Back-end | 0 | ABI 測試通過 | 稽核表與函式清單經 Architect 核可；ABI 測試通過 |
| W1 | 基礎：雙模式測試執行器、md5 輔助、處理函式表、記憶體計數器、生命週期（wait、wait_async、exit、clean、timeout）、PoC-001 用到的函式 | Back-end，QA 驗 | 約 12 | 執行器自測、A13 四項雜湊規則 | A6 至 A8、A10 不退化、A11 至 A14 完整驗證 |
| W2 | 事件與字串：get_*、get_*_at、return_*、`interface_*` 事件版、encode、decode、free、get_mime_type | Back-end，QA 驗 | 約 34 | 每函式一個測試；encode 與 decode 的釋放路徑 | A6 至 A8、A10、A11 至 A14 |
| W3 | 顯示、瀏覽器、腳本：show 系列、start_server、navigate、get_url、run、script、send_raw 及其 client 與 interface 版 | Back-end，QA 驗 | 約 22 | 需 Xvfb 的整合測試；script 緩衝區釋放 | 同上 |
| W4 | 視窗管理：size、position、kiosk、icon、focus、minimize 等 | Back-end，QA 驗 | 約 18 | 逐項呼叫並檢查回傳 | 同上 |
| W5 | 設定、網路、資料夾：set_config、profile、proxy、TLS、port、root_folder、last_error | Back-end，QA 驗 | 約 14 | 逐項測試；保留字串的釋放 | 同上，並做全量 valgrind |

## 8. 雙模式測試

- 模式 1（內建）：測試函式直接連入 exe。
- 模式 2（動態）：exe 以 `hb_hrbLoad` 載入 `.hrb`，再以 `hb_hrbRun` 或 `hb_hrbDo` 執行。
- 兩個模式使用同一份測試 `.prg`，結果必須相同。

### 8.1 雜湊規則

- 雜湊對象是 `.prg` 檔案的原始位元組，用 `hb_md5file()`。
- 參考雜湊存放在旁檔 `<name>.hrb.md5`。第一行是 32 位小寫十六進位 md5，第二行是 `hb_Version()`，Harbour 升級時會觸發重編。
- 編譯成功後才寫旁檔。
- 判斷順序：
  1. 無 `.prg`、有 `.hrb`：直接用 `.hrb`。
  2. 有 `.prg`、無 `.hrb` 或無旁檔：重編。
  3. 兩者皆有：旁檔與目前 `.prg` 的雜湊不同則重編，相同則沿用。
- 選用 md5 而非 crc32。兩者在 Harbour 核心都有（`hbchksum.h` 的 `hb_crc32`、`hb_md5`、`hb_md5file`），但 crc32 只有 32 位元，意外碰撞可能性較高。檔案很小，md5 的成本可忽略。
- 限制：只雜湊 `.prg`，不含 `.ch`。`.ch` 變動時需刪除旁檔。換行符變動也會觸發重編，屬可接受。

### 8.2 執行期編譯

- 呼叫 `hb_compileBuf( "harbour", "-n", "-w", "-q0", "-gh", cPrg )`，回傳 `.hrb` 位元組字串，失敗為 NIL。寫檔後再 `hb_hrbLoad`。
- 連結需加 `-lhbcplr -lhbpp -lhbcommon`。
- `std.ch` 已內建，實測故意給不存在的 `-I` 仍可編譯。
- 只有自訂 `.ch` 或 `hbclass.ch` 才需要以 `-I` 隨 exe 附帶。`hbapi.h` 只在 `.prg` 內含 C 傾印段時才需要，測試不使用。
- 已在 Linux（Harbour 3.2.0）以小型實測確認：`hb_compileBuf`、`hb_hrbLoad`、`hb_hrbDo` 可用，單執行緒與 `-mt` 連結皆通過。

### 8.3 bridge 影響

- `-fullstatic` 只改變靜態與動態函式庫的選擇，不會把未引用函式連入。
- 需要一個產生出來的 `bridge_request.prg`，內容是每個 `WEBUI_*` 的 `REQUEST` 行（或 hbmk2 的 `-request=`），讓符號進入 exe 的符號表。
- 另加一支 `.hrb` 逐一檢查每個函式都能解析。

### 8.4 新增工作量

新增 `bridge_request.prg`（約 100 行，可由函式清單產生）、雜湊與載入輔助 `.prg`（約 60 行）、測試執行器與兩個建置腳本。不改動 bridge 的 C 檔。

## 9. 核准與修訂紀錄

- [x] 使用者核准本規格（選項 A，分波交付）與雙模式測試（2026-11-26）。

| 修訂 | 日期 | 內容 | 狀態 |
|---|---|---|---|
| 0 | 2026-11-26 | 初版：完整 API 綁定、記憶體回收、分波計畫、雙模式測試 | 使用者核准，2026-11-26 |

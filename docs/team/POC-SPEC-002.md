# PoC-002：WebUI 完整 API 的 C89 綁定與記憶體回收

- 狀態：**已核准**（使用者於 2026-11-26 核准：選項 A，範圍內函式完整綁定、分波交付，每波經 QA 通過後才進下一波；雙模式測試已核准；Windows 延後，以 Linux 為準）
- 負責人：PM（規格）、Architect（做法）、Back-end（實作）、QA（驗收）
- 相關文件：[CHARTER.md](CHARTER.md)、[POC-SPEC.md](POC-SPEC.md)（PoC-001，A1 至 A5 不變）、[ENVIRONMENT.md](ENVIRONMENT.md)

## 1. 範圍

`include/webui.h`（2.5.0-beta.4）共 112 個公開函式。數量依 W0 稽核（[AUDIT-POC002-W0.md](AUDIT-POC002-W0.md)）定案：範圍內 104 個，排除 8 個。

| 分組 | 數量 | 內容 |
|---|---|---|
| 視窗 | 28 | new_window、destroy、minimize、maximize、size、position、kiosk、icon、focus、profile、proxy、`get_hwnd` 系列等 |
| 綁定與腳本 | 17 | bind、set_context、get_context、run、script、send_raw、navigate 及其 client 與 interface 版 |
| 顯示與瀏覽器 | 10 | show 系列、start_server、open_url、browser_exist |
| 等待與結束 | 9 | wait、wait_async、close、exit、clean、timeout、close_client |
| 字串與回傳 | 24 | get_*、get_*_at、return_*、encode、decode、free、`interface_*` 版 |
| 設定 | 2 | set_config、set_event_blocking |
| 網路與連接埠 | 7 | get_url、port、public、mime_type、TLS |
| 資料夾 | 3 | root_folder、default_root_folder、browser_folder |
| 其他 | 4 | 行程編號、last_error 系列 |
| 合計 | 104 | |

**排除（共 8 個）：**

| 函式 | 原因 |
|---|---|
| `webui_malloc`、`webui_memcpy` | 配置器輔助函式，Harbour 端沒有用途 |
| `webui_set_logger`、`webui_set_close_handler_wv`、`webui_set_file_handler`、`webui_set_file_handler_window` | 需要把回呼傳到 Harbour，並回傳不屬於 Harbour 的記憶體 |
| `webui_return_http`、`webui_interface_set_response_file_handler` | 同上 |

範圍內共 104 個函式。

**視窗控制代碼（修訂 1 納入範圍，W4）：** `webui_get_hwnd` 與 `webui_win32_get_hwnd` 回傳不透明的作業系統視窗控制代碼，不是堆積記憶體，bridge 不擁有它，**不得釋放**。Windows 上為 HWND；Linux 上 `webui_get_hwnd` 只在 WebView 模式回傳 GtkWindow*，其餘為 NULL；macOS 為 NULL。包裝函式以 `hb_retptr` 回傳 Harbour 指標項目，NULL 時改回傳 NIL。建議用指標項目而非 `hb_retnint` 數字：指標項目不能被誤做算術，也不受 2^53 精度限制；hbwin 等 Win32 函式本來就接受指標項目，需要數字時再用 Harbour 端轉換。

**取代而非排除：** 取用 `webui_event_t*` 的函式，改用 `interface_*` 版（以視窗與事件編號為參數）。Harbour 端不接觸任何指標。

## 2. 架構

- 檔案：`bridge_decl.h`（API 宣告）、`bridge_core.c`、`bridge_win.c`、`bridge_event.c`、`bridge_misc.c`，另有獨立的 `bridge_i64.c`（C99，見 4）。
- 每個函式一個 `HB_FUN_WEBUI_*`。
- 視窗與 ID 都用 Harbour 數字。
- 64 位元整數改用 `double`（`hb_parnd`、`hb_retnd`），精度上限 2^53。從 `double` 轉成 `size_t` 的輸入一律先檢查範圍（負值、NaN、超過 2^53 皆回報錯誤，不得直接轉型）。
- 處理函式從 PoC-001 的單一處理函式改為**表格**，以 bind id 為索引。保留舊的 `WEBUI_BINDHB` 簽名。

### 2.1 綁定設計規則（修訂 2，依 W0 稽核，具約束力）

1. 綁定回呼是 C 的靜態轉接函式（trampoline），依 bind id 查表；不得把 Harbour 符號或閉包直接交給 WebUI。
2. 表格項目只在 `webui_destroy` 或 `webui_clean` 之後釋放，不得在回呼之內釋放。
3. 建議採 `asynchronous_response` 加主執行緒事件佇列：轉接函式只把（視窗、事件編號、bind id）放入有互斥鎖的佇列，由 Harbour 主迴圈取出、執行處理函式，並在最後呼叫 `return_*`。在此模式下事件資料在 `return_*` 之前一直有效。
4. 回呼內不建議使用 `webui_script`（會阻塞回呼執行緒，`ui_event_blocking` 模式下還會讓其他事件排隊；逾時為 0 可能永久等待），無回傳值的呼叫改用 `webui_run`。
5. `webui_clean` 在回呼內不會清理，只會要求結束。必須在 `webui_wait` 返回後，於非回呼執行緒再呼叫一次。
6. `webui_set_event_blocking` 對視窗實際無效，改對應到 `webui_set_config(ui_event_blocking, ...)`。
7. `webui_set_tls_certificate` 在非 TLS 靜態庫上會使行程中止（`WEBUI_ASSERT`），bridge **必須**攔截並回傳 false，不得呼叫。
8. `webui_interface_is_app_running` 只能在第一次顯示之後使用，否則會鎖定為 false。
9. 呼叫端必須驗證範圍：`set_timeout` 0 至 60、`script` 逾時至多 86400、視窗大小 100 至 3840 乘 100 至 2160、位置 0 至 3000 乘 0 至 1800、引數索引至多 16。
10. 凡會顯示、等待或執行 script 的呼叫，一律在 Harbour 主執行緒進行（Linux WebView 模式在呼叫執行緒上運行 GTK）。
11. `webui_set_context` 與 `webui_get_context`（修訂 3 納入範圍，W2）以 bridge 內的槽位表實作：Harbour 值複製到 bridge 擁有的儲存空間，傳給 WebUI 的 `void*` 是槽位編號，不是位址；`get_context` 依槽位編號取回。同一個 window 與 element 重設時釋放舊槽位，`webui_clean` 時釋放全部槽位。`webui_set_context` 內部會呼叫 `webui_bind(window, element, NULL)`，會建立或改動綁定，必須有測試涵蓋此副作用。

## 3. 記憶體回收

- **必須釋放：** 只有 `webui_encode` 與 `webui_decode` 回傳堆積記憶體（原始碼已確認）。用 `hb_retc` 複製後，立即呼叫 **`webui_free`** 釋放。不使用 `hb_retc_buffer`，因為它會用 `hb_xfree`，與 WebUI 的配置器不同。
- **不得釋放：** 事件字串（`get_string`、`get_string_at`、`interface_get_string_at`）、`get_url`、`start_server` 的回傳、`get_mime_type`、`get_last_error_message` 回傳的字串屬 WebUI，只能用 `hb_retc` 當場複製，不得保存或釋放。
- **exit 之後：** `webui_free` 在 `webui_exit` 之後是空操作。encode 與 decode 的結果必須在 exit 之前釋放，否則只能由 `webui_clean` 回收。`webui_clean` 之後所有 WebUI 擁有的指標都失效。重新建立視窗的流程須在 W1 實測，不可假設。
- **bridge 暫存：** 例如 `webui_script` 的緩衝區，用 `hb_xgrab` 取得，複製後 `hb_xfree`。輸入字串只在呼叫期間有效。若 WebUI 會保留指標（例如 `set_custom_parameters`），bridge 先複製並登記，再次設定或清理時釋放。W0 稽核結果：WebUI 只保留函式指標（`webui_bind`、`webui_interface_bind`），所有字串與緩衝區引數都被複製或只在呼叫期間使用。
- **結束：** Harbour 在 `WEBUI_WAIT` 返回後呼叫 `WEBUI_CLEAN`，它呼叫 `webui_clean` 並釋放處理函式表。`webui_clean` 若在回呼內呼叫，只會要求結束，因此必須在 wait 之後呼叫（原始碼已確認）。
- **證明：**
  - valgrind 的 `--leak-check=full`。
  - 除錯版的配置與釋放計數器，結束後須為 0。

## 4. C89 規則

- 不使用 `//`；宣告放在區塊開頭；不使用 `long long`；不 include 任何 C99 標頭。
- 唯一例外是 `bridge_i64.c`：以 C99 撰寫，只放四個 64 位元整數函式（`get_int`、`get_int_at`、`interface_get_int_at`、`return_int`），不 include `webui.h`，自行宣告原型，並以 `-std=c99 -pedantic-errors` 編譯。其餘檔案維持 C89。
- `webui.h` 與 `hbapi.h` 仍不 include，與 PoC-001 一致，因為兩者含 C99 語法。bridge 自行宣告所用子集。
- `bool` 回傳宣告為 `unsigned char`。參數以 `int` 傳入。
- 另做一次 C99 對照編譯，確認宣告與真實標頭沒有衝突。

## 5. 驗收

| # | 通過條件 |
|---|---|
| A6 | 每個範圍內函式可由 Harbour 呼叫，並有對應測試 |
| A7 | 除 `bridge_i64.c`（以 `-std=c99 -pedantic-errors` 驗證）外，所有 bridge 檔案通過 `gcc -std=c89 -pedantic-errors` |
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

工作量（技術描述）：新增 6 個 C 檔案、約 104 個包裝函式（其中 4 個在 C99 檔）、每組一支測試 `.prg`，另有 `bridge_request.prg`、雜湊與載入輔助、測試執行器與兩個建置腳本。需稽核約 20 個保留字串的函式。不動 `webui.c`，只改 `bridge.c` 的結構與 `main.prg`。

## 7. 分波計畫

每波由 Back-end 實作、QA 驗證，通過後才進下一波。函式數依 W0 稽核定案，合計 104。

| 波 | 內容 | 負責 | 函式數 | 必要測試 | 出口標準 |
|---|---|---|---|---|---|
| W0 | 前置：字串保留稽核、`bool` 與 64 位元 ABI 測試、`bridge_request.prg` 產生器與函式清單 | Architect、Back-end | 0 | ABI 測試通過（見 7.1） | 稽核表已完成；函式清單經 Architect 核可；ABI 測試通過 |
| W1 | 基礎：雙模式測試執行器、md5 輔助、處理函式表、記憶體計數器、生命週期（new_window、bind、show、wait、wait_async、close、exit、clean、set_timeout、set_config）、get_string、return_string | Back-end，QA 驗 | 12 | 執行器自測、A13 四項雜湊規則、clean 後重建視窗 | A6 至 A8、A10 不退化、A11 至 A14 完整驗證 |
| W2 | 事件與字串：get_count、get_*_at、return_int、return_float、return_bool、encode、decode、free、last_error、`interface_bind`、`interface_*` 取值與回應、is_app_running、get_window_id、set_context、get_context | Back-end，QA 驗 | 29 | 每函式一個測試；encode 與 decode 的釋放路徑；int64 檔；set_context 的 bind(NULL) 副作用、槽位重設與 clean 釋放 | A6 至 A8、A10、A11 至 A14 |
| W3 | 顯示、瀏覽器、腳本：new_window_id、show 系列、start_server、get_url、open_url、run、script、send_raw、navigate、set_runtime、close_client 及其 client 與 interface 版 | Back-end，QA 驗 | 27 | 需 Xvfb 的整合測試；script 緩衝區釋放 | 同上 |
| W4 | 視窗管理：size、position、kiosk、icon、focus、minimize、maximize、destroy、profile、proxy、set_event_blocking、get_hwnd、win32_get_hwnd 等 | Back-end，QA 驗 | 25 | 逐項呼叫並檢查回傳；`get_hwnd` 系列在 Linux 測無效視窗編號與非 WebView 模式回傳 NIL（真正的 HWND 延後到 Windows 驗證） | 同上 |
| W5 | 資料夾、網路、其他：root_folder、default_root_folder、browser_folder、port、public、get_free_port、mime_type、TLS（被攔截並回傳 false）、行程編號 | Back-end，QA 驗 | 11 | 逐項測試；TLS 攔截不得使行程中止 | 同上，並做全量 valgrind |

合計：12 + 29 + 27 + 25 + 11 = 104。

### 7.1 W0 出口標準：ABI 測試

完整設計見 [AUDIT-POC002-W0.md](AUDIT-POC002-W0.md) 第 6 節。W0 通過需 A 至 E 五組全部通過：

| 組 | 內容 |
|---|---|
| A | 靜態檢查：型別大小、事件結構配置、列舉數值、全部 112 個原型與 bridge 宣告零差異 |
| B | `bool` 回傳只用低位元組（反組譯檢查，僅供參考） |
| C | `bool` 回傳與參數、列舉參數的實際執行（含暫存器污染對照），1000 次零不一致 |
| D | `size_t` 與 `double` 往返、實際 ID 範圍、64 位元整數往返 |
| E | Xvfb 加 Chrome 的實際回呼、`webui_script`、非同步回應 |

Linux 執行指令：

```
make
gcc -std=c89 -pedantic-errors -Wall -Wextra -O2 -c abi_bridge.c
gcc -std=c99 -O2 -Iinclude -c abi_real.c abi_i64.c
gcc abi_bridge.o abi_real.o abi_i64.o dist/libwebui-2-static.a -lpthread -lm -ldl -o abi_test
./abi_test --headless
xvfb-run -a -s "-screen 0 1280x1024x24" ./abi_test --live
```

測試矩陣為 {-O0, -O2} 加 ASan 與 UBSan 組建。Win32 重跑延後。

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
| 1 | 2026-11-26 | `webui_get_hwnd`、`webui_win32_get_hwnd` 由排除改為範圍內（W4），以 `hb_retptr` 回傳、不釋放；排除 12 改 10，範圍內 100 改 102，W4 約 18 改約 20；Windows 真實 HWND 測試延後 | 依使用者提問修訂，2026-11-26 |
| 2 | 2026-11-26 | 依 W0 稽核：範圍內 102、排除 10；各波函式數改為 12、27、27、25、11（合計 102）；新增 2.1 綁定設計規則；`bridge_i64.c` 為唯一 C99 檔，A7 相應調整；新增 7.1 W0 出口標準（ABI 測試 A 至 E）；記憶體規則補充 exit 之後與 WebUI 擁有字串的處理 | 依 Architect 稽核修訂，2026-11-26 |
| 3 | 2026-11-26 | `webui_set_context`、`webui_get_context` 由排除改為範圍內（W2），以 bridge 槽位表實作；排除 10 改 8，範圍內 102 改 104；各波函式數為 12、29、27、25、11（合計 104） | 使用者核准，2026-11-26 |

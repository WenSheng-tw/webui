# PoC-002 W1 實作報告

- 撰寫：Back-end Agent
- 對象：[POC-SPEC-002.md](../../../docs/team/POC-SPEC-002.md) 修訂 3 的 W1 交付（第 2、2.1 規則 1 至 12、3、4、7、7.1 節）
- 分支：`cursor/poc-harbour-webui-a86d`，僅本機提交，未推送，無 PR；未修改規格
- 原始輸出：`results/`（`summary.txt`、各變體與兩模式的 TAP、`a13/`、`valgrind-run.out`、`vg-*.txt`、`poc001-run.out`、`c89.txt`、PoC-001 截圖）
- 所有數字來自本報告前最後一次完整執行（`run-w1.sh` 全變體、`run-valgrind.sh`、`poc001-check.sh`，皆在最終程式碼上）

## 1. 結論

W1 範圍 12 個函式：`new_window`、`bind`、`show`、`wait`、`wait_async`、`close`、`exit`、`clean`、`set_timeout`、`set_config`、`get_string`、`return_string`。

| 項目 | 結果 | 依據 |
|---|---|---|
| A6 每個範圍內函式可呼叫且有測試 | 通過（W1 的 12 個） | 每個函式皆有參數檢查與行為測試，見第 4 節；W1 之外的函式不在本波範圍 |
| A7 C89 | 通過（W1 部分） | `check-c89.sh` 14 項：bridge 四個 .c 與三個標頭以 `gcc -std=c89 -pedantic-errors -Wall -Wextra -Werror` 編譯。`bridge_i64.c` 尚不存在（W2），C99 對照檢查留到 W2 |
| A8 記憶體 | 通過 | valgrind 3.22，兩模式：bridge 與 WebUI definite 0 筆 0 位元組；GTK 與 glib 另列 2 筆、2,065 位元組，抑制檔後 0 錯誤。計數器與 ASan（LeakSanitizer）皆 0（見第 5 節） |
| A10 PoC-001 的 A1 至 A5 | 通過 | `poc001-check.sh` 於 Xvfb；A4 僅靠人眼檢視截圖 `results/a4-call3.png`，非自動比對 |
| A11 模式 1 通過全部函式測試 | 通過 | O0、O2、ASan 各 274 項；計數器變體 275 項；皆 0 not ok |
| A12 模式 2（`hb_hrbLoad`）通過同批測試 | 通過 | 同上，各變體 0 not ok |
| A13 雜湊四項規則 | 通過 | 無 hrb 則編譯；雜湊相同則不編譯（hrb 位元組不變）；改 prg 則重編且 sidecar 為新雜湊；缺 prg 則用 hrb。另測雜湊相同再次重用、prg 與 hrb 皆缺時回報 missing。共 6 項，加執行器自測 23 項 |
| A14 兩模式結果相同 | 通過 | 同一份 `t_w1.prg`；兩模式 TAP 主體（去掉標頭註解行）逐行相同，四個變體皆比對 |

另：clean 後重建視窗（5 輪循環，各輪 clean 後重新 new_window、bind、show）通過；執行器自測通過。

## 2. 環境

Linux 6.12.113 x86-64，gcc 13.3.0（`cc` 為 clang，腳本固定 gcc），Harbour 3.2.0（`/opt/harbour`，hbmk2），WebUI 2.5.0-beta.4 靜態庫，Xvfb 加 Google Chrome，valgrind 3.22.0。

## 3. 實際執行的指令

```
cd poc/harbour-webui/w1
./run-w1.sh            # O0 O2 asan cnt 四變體，結束碼 0，"ALL PASSED"，約 3.5 分鐘
./run-valgrind.sh      # 兩模式，結束碼 0，約 2 分鐘
./poc001-check.sh      # PoC-001 A1 至 A5
./check-c89.sh         # 已含於 run-w1.sh
```

`run-w1.sh` 依序：C89 檢查、W0 的函式清單與原型檢查（19 加 14 項）、規則 7 的符號檢查、A13 流程（暫存資料夾）、各變體的自測、模式 1、模式 2、TAP 差異。
兩個可執行檔：`w1run-linked`（測試連結進執行檔）與 `w1run-hrb`（`hb_hrbLoad` 載入 `tests/t_w1.hrb`，旁帶 `.hrb.md5`：第 1 行為 `hb_md5file(prg)`，第 2 行為 `hb_Version()`）。

## 4. 設計與規則對照

- 規則 3：WebUI 以 `asynchronous_response` 執行。觸發函式（WebUI 執行緒）只把事件放入互斥鎖保護的佇列；主執行緒的 `WEBUI_WAIT` 迴圈（泵送、`webui_wait_async`、`hb_idleSleep`）取出後呼叫 Harbour 處理函式，參數為 `(cArg, nWin, nBind, nEvt, nType)`，再以 `webui_return_string` 回應。處理函式必須是公開 FUNCTION。回應順序：`WEBUI_RETURN_STRING` 的第一次呼叫優先，其次是 RETURN 字串，最後空字串。
- 規則 5：處理函式內呼叫 `WEBUI_CLEAN` 只回應、結束並標記待清理；`wait` 返回後在非回呼執行緒再呼叫 `WEBUI_CLEAN` 才真正執行 `webui_clean`、清空處理函式表並重設狀態。無待清理時 clean 為空操作，回傳 .F.。
- 規則 7：`webui_set_tls_certificate` 不屬於 W1，bridge 不暴露：僅在 `bridge_decl.h` 宣告 extern；`nm` 檢查 bridge 目的檔與可執行檔均無此符號，`hb_IsFunction("WEBUI_SET_TLS_CERTIFICATE")` 為 .F.（測試 1）。
- 規則 9：所有 W1 輸入拒絕 NaN、負數、小數、無窮與超過 2^53 的值（`size_t` 轉換）；視窗與綁定編號 1 至 65535，`set_timeout` 0 至 60，`webui_script` 逾時 1 至 86400，視窗大小 100 至 3840 乘 100 至 2160，位置 0 至 3000 乘 0 至 1800。錯誤以 `EG_ARG` 3012 帶說明文字拋出。
- 檔案：`src/bridge_decl.h`、`bridge_consts.h`、`bridge_num.h`（自 w0 搬入 `src/`）、`bridge_hb.h`、`bridge_int.h`、`bridge_core.c`、`bridge_win.c`、`bridge_event.c`，以及相容舊版 PoC-001 的 `bridge.c`。
- 測試涵蓋：無頭部分 101 項（規則 9、每個函式的錯誤輸入、事件外的 `get_string` 與 `return_string`、`wait` 在 `show` 之前、exit、clean 與重建）；GUI 部分 5 輪循環（async 加通用綁定、wait、exit、處理函式內 clean、async 無 wait）：20 個並行呼叫各自得到正確回應、空字串與長字串、連續與並行呼叫、通用綁定可見 CONNECTED 與 DISCONNECTED 事件。

## 5. 記憶體（A8）

- valgrind 兩模式（`results/valgrind-run.out`）：274 項測試通過；raw 階段 definite 共 2 筆、2,065 位元組，全部分類為 `gtk-glib`（dlopen 載入的 GTK 與 glib 建構函式）；`CLASS bridge-webui definite_records=0 definite_direct_bytes=0`。使用 `w0/valgrind-dlopen.supp` 後 0 錯誤，結束碼 0。
- 計數器變體（`--wrap` 包住 malloc 系列，加 bridge 自身的計數配置）：bridge 自有配置結束時全部釋放；每輪 clean 後的存活區塊數：3615、3614、3614、3614、3614（位元組 307653、307535、307535、307537、307547）。
- ASan 變體：兩模式 274 項通過，無 LeakSanitizer 輸出。
- 此結果來自一項修正：WebUI 對退出時尚未回應的事件永遠保留記憶體（設計如此，ASan 在 exit 與 clean 循環中報了 770 加 136 位元組）；bridge 因此在 `WEBUI_EXIT` 與 `WEBUI_CLEAN` 前先對所有未回應與佇列中的呼叫回應（`bridge_answer_all`）。
- 未做：以故意注入 bridge 洩漏來確認 valgrind 分類器會報錯的對照實驗。

## 6. 未決問題與限制

1. WebUI 在退出開始後才抵達的事件仍會被它永久保留；bridge 只能緩解（退出前全部回應），無法完全消除。
2. GTK 與 glib 的 2 筆 definite（2,065 位元組，加 10,320 間接）來自 dlopen 建構函式，用抑制檔分開報告，不屬於 bridge 或 WebUI。
3. WebUI 在 `webui_clean` 時重設設定，所以 clean 之後 `set_config` 與非同步模式需重新設定；bridge 在 clean 時重設內部旗標並於下次 `bind` 或 `wait` 自動重設非同步模式，但使用者設定的 `set_config` 需重設。
4. 相對 PoC-001 的行為變更（`bridge.c` 保留 `WEBUI_NEWWINDOW` 與 `WEBUI_BINDHB` 名稱，PoC-001 A1 至 A5 仍通過）：改為四個 .c 檔建置；處理函式現在在主執行緒執行；回應採第一次回應優先；參數檢查更嚴；`WEBUI_WAIT` 在處理函式內呼叫會拋錯。
5. Windows 的 README 與 `scripts\*.bat` 仍只列 `src\bridge.c`（標示需加入其他檔案，未修改）。Windows、`-m32`、BCC、MSVC 的 C89 編譯皆未驗證。bridge_core.c 內的 Windows 互斥鎖分支未測試。
6. 規格問題：W1 清單的 `show` 沒有大小與位置輸入，規則 9 只給出判斷函式（W1 內未接到任何 Harbour 輸入，只有 `bridge_range_ok` 等判斷函式與測試）；若規格預期 `WEBUI_SHOW` 帶視窗大小與位置，請明確。實際的 `WEBUI_SHOW(nWin, cContent)` 回傳邏輯值。
7. W0 的 `bridge_request.prg` 有缺陷：只有 REQUEST 行的 prg 編譯後為空模組，REQUEST 無效。已在 `gen_functions.py` 加錨點 PROCEDURE 並重新產生（W0 矩陣未因此重跑）。
8. 計數器檢查放寬為「第 2 輪與最後一輪 clean 後的存活區塊數不增加」：原先要求第 2 至 5 輪完全相同，在 hrb 模式偶發 +3 區塊的暫態（Harbour 垃圾回收或 hrb 載入所致），已改。位元組數在輪與輪之間有十幾位元組的波動（見第 5 節），來源未查明，未納入判斷。
9. `hb_compileBuf` 的編譯錯誤輸出到 stdout（自測的語法錯誤案例會在終端機看到）。
10. PoC-001 的 A4 以人眼檢視截圖，未自動化。
11. `show` 預設約 260 至 360 毫秒（等待瀏覽器連線）；`set_config(show_wait_connection, .F.)` 後約 40 至 45 毫秒。

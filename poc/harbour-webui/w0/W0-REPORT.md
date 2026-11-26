# PoC-002 W0 實作報告

- 撰寫：Back-end Agent
- 對象：[POC-SPEC-002.md](../../../docs/team/POC-SPEC-002.md) 修訂 3 的 W0 交付（2.1、4、7、7.1）與 [AUDIT-POC002-W0.md](../../../docs/team/AUDIT-POC002-W0.md) 第 6 節
- 分支：`cursor/poc-harbour-webui-a86d`，僅本機提交，未推送
- 重跑基準：HEAD `71a214f`（含 `e0067b4`，範圍內 104）加上其後的計數器與 valgrind 提交；矩陣與 valgrind 都在這份程式碼上重跑，第 3、4 節數字皆來自最後一次完整執行
- 原始輸出：`results/*.tap`、`results/run-abi-summary.txt`、`results/valgrind-*.log`、`results/valgrind-classify.txt`

## 1. 結論

| 項目 | 結果 |
|---|---|
| A 靜態與配置 | 通過（A1 至 A4，112 個原型全部比對，零差異） |
| B 反組譯 | 已執行，僅供參考，無通過與否 |
| C bool 與列舉 | 通過（含暫存器污染對照，各案例 1000 次零不一致） |
| D 數值往返 | 通過（D1、D2、D3 含實際 10000 個事件） |
| E 實際回呼 | 通過（Xvfb 加 Chrome，O0、O2、ASan 與計數器變體各一次） |
| A8 記憶體 | valgrind 3.22：bridge 與 WebUI 的 definite 洩漏 0 筆 0 位元組；GTK 與 glib（dlopen 建構函式）2 筆、2,065 位元組，由抑制檔分開列出。計數器（253,456 次配置與釋放）與 LeakSanitizer 皆為 0（見 4.1） |
| 函式清單 | 完成，112 筆，12 / 29 / 27 / 25 / 11 / 排除 8，範圍內合計 104（修訂：CEO 決定 `webui_set_context`、`webui_get_context` 納入 W2） |

W0 出口標準第一項（ABI 測試 A 至 E）在本機 Linux x86-64 通過。未發現會阻擋 W1 的 ABI 失敗。有數項稽核與實測不一致或稽核未涵蓋的發現，見第 5 節。

## 2. 環境

- Linux 6.12.113 x86-64，gcc 13.3.0，Python 3.12.3，GNU objdump 2.42
- Google Chrome 157.0.8103.50，`xvfb-run` 與 Xvfb 可用（PoC-001 同一方式，見 [TEST-REPORT-POC001-LINUX.md](../../../docs/team/TEST-REPORT-POC001-LINUX.md)）
- 靜態庫：`dist/libwebui-2-static.a` 原本不存在，在倉庫根目錄執行 `make` 產生（非 TLS、非 WebView 變體）
- 本機 `cc` 是 clang，Makefile 固定使用 gcc

## 3. 實際執行的指令

```
cd poc/harbour-webui/w0
./run-abi.sh        # 約 6 分 6 秒，結束碼 0，輸出 "RESULT: all runs passed"
./run-valgrind.sh   # 約 4 分，結束碼 0（見 4.1）
```

`run-abi.sh` 依序執行：

1. `python3 gen_functions.py`、`python3 check_protos.py`、`python3 check_bool_abi.py`
2. 對 `O0`、`O2`、`asan`、`cnt` 四個變體：`make build-<變體>`，再執行 `abi_test --headless`，再執行 `xvfb-run -a -s "-screen 0 1280x1024x24" abi_test --live --dir <w0>`

編譯旗標（`Makefile`）：

| 檔案 | 旗標 |
|---|---|
| `abi_bridge.c` | `-std=c89 -pedantic-errors -Wall -Wextra`，所有變體皆零警告 |
| `abi_i64.c` | `-std=c99 -pedantic-errors -Wall -Wextra`，零警告 |
| `abi_real.c`、`abi_counters.c` | `-std=c99 -Wall -Wextra`，零警告 |
| asan 變體 | `-O1 -g -fsanitize=address,undefined,float-cast-overflow -fno-sanitize-recover=all`（任何未定義行為即中止） |
| cnt 變體 | `-O2 -g -DABI_COUNTERS`，連結 `-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=free` |
| vg 變體（valgrind 用） | `-O0 -g`，無 sanitizer、無 wrap |

結果摘要（`results/run-abi-summary.txt`，HEAD `71a214f` 之上）：

```
functions     rc=0  ok=19  not_ok=0
protos        rc=0  ok=14  not_ok=0
bool-objdump  rc=0  ok=23  (全部為 informational)
O0-headless   rc=0  ok=58  (informational 14)  not_ok=0
O0-live       rc=0  ok=33  (informational 1)   not_ok=0
O2-headless   rc=0  ok=58  (informational 14)  not_ok=0
O2-live       rc=0  ok=33  (informational 1)   not_ok=0
asan-headless rc=0  ok=58  (informational 14)  not_ok=0
asan-live     rc=0  ok=33  (informational 1)   not_ok=0
cnt-headless  rc=0  ok=59  (informational 13)  not_ok=0
cnt-live      rc=0  ok=34  (informational 0)   not_ok=0
```

各變體的 ok 數不同，是因為 cnt 變體多了計數器的 2 項檢查，其餘變體多 1 項「計數器未啟用」的資訊項。與前一次執行（57 與 32 項）相比，沒有任何項目轉為失敗。

## 4. 各組結果

### A 靜態與配置

| ID | 內容 | 結果 |
|---|---|---|
| A1 | 真實標頭：`sizeof(bool)=1`、`webui_config=4`（等於 int）、`size_t=void*=8`、`long long=8` | 通過；C99 單元用負陣列大小做編譯期斷言，C89 單元做同類斷言並於執行期比對 |
| A2 | 鏡像 `webui_event_t` 與真實版：大小 64，8 個欄位偏移 | 通過，0 不一致。變異測試：把鏡像的 `client_id` 與 `connection_id` 對調，A2 回報 2 個不一致（偏移 40 與 48），確認檢查有效 |
| A3 | 32 個列舉值（config 6、browser 14、runtime 4、event 5、logger 3）：`bridge_consts.h` 的手寫巨集對真實標頭 | 通過。`check_protos.py` 做文字漂移檢查（缺、多、值不同皆失敗），`abi_real.c` 用 `abi_enums_gen.h`（由標頭產生）逐一比對執行期數值 |
| A4 | 112 個 `WEBUI_EXPORT` 原型全部由腳本分類與比對 | 通過：104 個範圍內與 `bridge_decl.h`（100 個）加 `abi_i64.c`（4 個，`long long`）零差異、零缺漏、零多餘；8 個排除函式皆未宣告（`set_context`、`get_context` 改列範圍內後於 `bridge_decl.h` 補宣告並重跑 A4） |

A4 的正規化規則：回傳 `bool` 變 `ubool`，參數 `bool` 與 `webui_config` 變 `int`，`long long int` 變 `long long`，丟棄參數名稱，`()` 視為 `(void)`。腳本內建 3 個負向對照（把 bool 參數宣告成 `ubool`、刪掉一個宣告並多宣告一個排除函式、把 `unsigned int` 放寬為 `size_t`），都被偵測到。

### B 反組譯（僅供參考）

`check_bool_abi.py` 對 `libwebui-2-static.a` 的 20 個回傳 bool 的函式找出每個 `ret` 或尾端跳躍前最後寫入 rax 的指令。各類出口數：常數 0 共 7、複製自暫存器 `%ebp` 共 4、只寫低位元組（`mov` 到 `%al`）共 1、呼叫結果共 5、尾端呼叫共 8、`xor` 計算共 1。判讀：有 18 個出口的高位元組可能是未定義的，所以橋接層以 `unsigned char` 只讀低位元組是必要的，不能假設高位元組為零。`webui_set_tls_certificate` 沒有 `ret` 出口，與稽核的「非 TLS 版會中止」一致。

### C bool 與列舉

- C1：11 組真實案例，每組 1000 次（`is_high_contrast` 因每次要開子行程只做 20 次），三種呼叫方式逐次比對：經 `ubool` 宣告直接呼叫、經真實標頭 `bool` 呼叫（`abi_real.c`）、經組合語言轉接函式在呼叫前把 rax 填入 `0xDEADBEEFCAFEBABE`（暫存器污染對照）。涵蓋 `set_root_folder`（真與假）、`set_default_root_folder`（真與假）、`set_port`（使用中與空閒）、`is_shown`、`browser_exist`（Safari 為假、Chrome 為真）、`interface_is_app_running`、`is_high_contrast`。全部 0 不一致、0 個值落在 {0,1} 之外。
- 對照組有效性：另以一個只設定 `%al` 的測試函式驗證，污染後 `rax=0xdeadbeefcafeba01`，低位元組為 1，用 `int` 讀會錯，用 `ubool` 讀正確。
- 負向對照（資訊性）：上述 11 組實際函式在本機組出的函式庫上都沒有留下污染的高位元組（0 次），所以「把結果當 `int` 讀會錯」在目前這份二進位上沒有實際發生，但 B 與對照函式顯示它在 ABI 上是被允許的。
- C2：`webui_set_public` 的 bool 參數用 `int` 傳入，以 `/proc/net/tcp` 觀察監聽位址。`int 1` 得 `00000000`（0.0.0.0），`int 0` 得 `0100007F`（127.0.0.1），真實 bool 原型結果完全相同。`int 256` 得 127.0.0.1，即被當成假（只讀低位元組），這是文件化的不支援情況，bridge 必須用 `hb_parl() ? 1 : 0` 正規化。
- C3：六個 `webui_config` 值皆可經 `int` 宣告呼叫；觀察到的效果：
  - `use_cookies`：為真時回應含 `Set-Cookie: webui_auth=`，為假時沒有（無頭）
  - `show_wait_connection=0`：`webui_show` 在 約 40 ms 內返回，返回時 `is_shown=0`，約 251 ms 後變成 1
  - `ui_event_blocking`：0 時兩個同時事件的最大並行數為 2，1 時為 1
  - `multi_client=1`：`webui_script` 回傳假（與稽核一致），改回 0 後回傳真
  - `asynchronous_response`：見 E3
  - `folder_monitor`：**未觀察效果**（需要檔案監控與根目錄變更，W0 未實作），僅確認呼叫不當機
- C4（附帶）：`webui_encode("hello")` 為 `aGVsbG8=`，`webui_decode` 可還原，`webui_free` 可釋放；`webui_get_mime_type("index.html")` 為 `text/html`；`last_error` 為 0 與空字串。

### D 數值往返

- D1：`size_t` 與 `double` 往返，0、1、65535、2^31-1、2^32-1、2^53 全部精確；2^53+2、2^64、-1、NaN、1.5、正負無限大、-0.5 全部被 `bridge_n2size`（`bridge_num.h`）拒絕且不轉型；`SIZE_MAX` 的反向轉換被拒絕；-0.0 視為 0。asan 變體含 `float-cast-overflow`，沒有觸發。
- D2：200 個視窗，編號為 7 到 206，皆在 1 至 65535，無重複，精確往返。`new_window_id(60000)` 得 60000，`get_new_window_id` 得 207。實際（有瀏覽器）10000 個事件：`event_number` 最小 458759、最大 655763222（皆小於 2^31，精確往返 double），`bind_id` 最大 3，`client_id` 與 `connection_id` 為 0。
- D3：`abi_i64.c` 的轉換：|v| 不超過 2^53 為精確數字，超過則回傳溢位旗標與完整十進位字串。JS 送 `"9223372036854775807"`、`"-9223372036854775808"`、`"0"`、`"-1"`，`get_int_at`、`interface_get_int_at`、`get_int` 都精確取回；`return_int` 送出 LLONG_MAX、LLONG_MIN、0、-1，JS 收到的十進位字串精確相等；double 42 回 `"42"`；2^53+2 在送出前被拒絕。

### E 實際回呼（Xvfb 加 Chrome）

| ID | 內容 | 結果 |
|---|---|---|
| E1 | 頁面呼叫 `webui.call('abi_f', 123, 'x', true, 1.5)`，以鏡像結構與靜態 trampoline 接收 | 通過：`get_count`=4、`get_int_at(0)`=123、字串 `x`、布林 1、浮點 1.5、元素名、視窗編號、事件型別 4、bind id 皆正確；回呼執行緒不是主執行緒 |
| E2 | `webui_script("return 40+2;")` | 回傳真與 `42` |
| E2 | 擲出例外的腳本 | 回傳假；緩衝區先被清零，之後含錯誤文字（見 F2） |
| E2 | 逾時 1 秒的腳本（頁面忙碌 1.6 秒） | 約 1002 ms 返回假，緩衝區為空，1.5 秒後仍為空（遲到結果不會寫入） |
| E2 | 結果比緩衝區長 | 截成 size-1 |
| E3 | `asynchronous_response=1`，trampoline 只把（事件指標、視窗、事件編號、元素、第一個引數）放入有互斥鎖的佇列，主執行緒取出處理 | 通過：主執行緒在回應前能以 `interface_get_string_at` 取到事件資料；延遲 300 ms 後 `interface_set_response` 的值在 JS 端 345 ms 後收到；`return_bool(int 1)`、`return_bool(int 0)`、`return_float(2.5)` 在 JS 為 `"1"`、`"0"`、`"2.500000"`；佇列無溢位 |

四個變體的 E 結果相同。E 組只測瀏覽器模式，沒有測 WebView。

### 4.1 記憶體回收證據（A8）

三種方法用同一份原始碼、不同組建：valgrind 用 `vg` 變體（`-O0 -g`，未插樁），計數器用 `cnt` 變體，LeakSanitizer 用 `asan` 變體。valgrind 與 ASan 不能同時存在於同一個執行檔，因此「同一個二進位」無法字面成立，這是偏差。

**valgrind 3.22.0**（`./run-valgrind.sh`，無頭群組，C1 每案 1000 次，`--leak-check=full --errors-for-leak-kinds=definite`）：

| 項目 | 結果 |
|---|---|
| 測試本身 | 58 項全部 ok，結束碼 0 |
| 堆積用量 | 261,185 次配置、260,902 次釋放，結束時仍有 283 個區塊 |
| 第一輪（無抑制檔）definite | 共 2 筆（1 位元組 1 區塊；2,064 位元組直接加 10,320 位元組間接）。`vg_classify.py` 依堆疊含 `_dl_init`（dlopen 載入的函式庫建構函式）分類 |
| 分類：**bridge 與 WebUI** | definite **0 筆、0 位元組**，possible 0 筆 |
| 分類：**GTK 與 glib（dlopen）** | definite 2 筆、2,065 位元組，間接 10,320 位元組 5 區塊；堆疊為 `g_strdup`、`calloc` 經 `_dl_init`、`dlopen`、`_webui_load_gtk_and_webkit`、`webui_set_center`、`webui_new_window_id` |
| 第二輪（`valgrind-dlopen.supp`） | 結束碼 0；definitely lost 0、indirectly lost 0、possibly lost 0；錯誤 0（被抑制 2）；used_suppression：definite 與間接共 7 區塊 22,705 位元組，reachable 235 區塊 35,768 位元組 |
| still reachable（不算洩漏） | 8,265 位元組 20 區塊，堆疊皆在 ld.so 的 dlopen 登記（`_dl_new_object`、`_dl_load_cache_lookup`）與 glib 雜湊表，沒有任何 WebUI 或 test 的堆疊 |

抑制檔 `valgrind-dlopen.supp` 的比對條件要求堆疊中依序有 `_webui_load_gtk_and_webkit`、`dlopen`、`_dl_init`，所以 WebUI 或測試自己配置的記憶體不可能被它蓋住。valgrind 只跑了無頭群組，沒有跑 E 組。

**配置計數器**（`cnt` 變體，連結時以 `--wrap` 攔截測試物件與 `libwebui-2-static.a` 的 malloc、calloc、realloc、free，並追蹤每個存活區塊）：

| 群組 | `webui_clean` 之後 |
|---|---|
| 無頭 | 253,456 次配置、253,456 次釋放、存活 0 區塊 0 位元組、外來釋放 0、表格溢位 0 |
| E 組（含瀏覽器） | 50,434 次配置、50,434 次釋放、存活 0 區塊 0 位元組、外來釋放 0、表格溢位 0 |

計數器本身有對照：故意持有一個 77 位元組區塊，存活數由 0 變 1，釋放後回 0。計數器看不到 libc 內部與 dlopen 載入的函式庫配置，這部分由 valgrind 負責。

**LeakSanitizer**（`asan` 變體）：無頭與 E 組皆結束碼 0，標準錯誤輸出為空，沒有洩漏報告。LeakSanitizer 在本機有效（以小程式確認過）。函式庫本身未插樁，只攔截 malloc 家族。

結論：bridge 與 WebUI 的 definite 洩漏為 0（valgrind），計數器為 0，LeakSanitizer 無報告，三者一致。

## 5. 發現與偏差

| 編號 | 內容 | 證據 | 建議 |
|---|---|---|---|
| F1 | 稽核 E1 預期 `get_count`=5，實際為 4。`webui.call` 的第一個參數是函式名，不算引數；JS 傳的是 123、'x'、true、1.5 共 4 個。規格修訂 3 已採用 4 | `results/O2-live.tap` 第 14 項 | 已結案：以 4 為準，稽核表本身仍寫 5（未改） |
| F2 | `webui_script` 的旗標加緩衝區行為：成功為真加結果；JS 例外時回傳假，但緩衝區內是錯誤文字（本例為 `abi`），不是空的；逾時為假且緩衝區保持零，遲到的結果不會寫入。稽核只說「回傳假」 | `O2-live.tap` 第 9、10 項；`src/webui.c` 的 `webui_script_client` 回傳 `!js_error` | 已被規格修訂 3 的規則 12 採用（旗標加緩衝區共同表示：真為結果、例外為錯誤文字、逾時為空）；W3 的 `WEBUI_SCRIPT` 須同時回傳旗標與緩衝區，並以測試覆蓋三種情況 |
| F3 | 在目前二進位上沒有觀察到 bool 回傳的高位元組被污染（0 次），但 B 顯示 18 個出口可能留下未定義的高位元組 | `O2-headless.tap` 的負向對照與 `bool-objdump.tap` | 維持 `ubool` 只讀低位元組的規則，不可因為測不到污染而改回 `int` |
| F4 | 稽核表仍把 `webui_get_hwnd` 與 `webui_win32_get_hwnd` 列為排除，W4 為 23、範圍內為 100。依規格修訂 1 與 2，函式清單已改為 W4=25（本項之後又因 CEO 決定改為範圍內 104、排除 8，W2=29） | `functions.txt`、`gen_functions.py` | 請 Architect 更新稽核表（本次未改稽核文件） |
| F5 | 規格 4 的「C99 對照編譯確認宣告與真實標頭沒有衝突」無法在同一個翻譯單元內做，因為 `ubool`（unsigned char）與真實 `bool` 的宣告本來就衝突，`webui_event_t` 也重複 | — | 改為：`abi_real.c` 以真實標頭編譯（C99），加上 A4 腳本比對；若規格要字面的對照編譯，需要改規格 |
| F6 | 執行時間：O2 變體的無頭測試約 95 秒，主要是 `set_port` 與 `browser_exist(Chrome)` 各 3000 次呼叫；完整矩陣（四個變體）約 6 分 6 秒 | `run-abi.sh` 的 `time` 輸出 | 若 CI 嫌慢，可把這兩個案例降到 300 次，需先改規格的 1000 次要求 |
| F7 | A8 已有 valgrind、計數器與 LeakSanitizer 三份證據（見 4.1），三者一致。限制：valgrind 只跑無頭群組；計數器與 LeakSanitizer 只涵蓋這個測試流程；bridge 尚未存在，W1 起要對真正的 bridge 重做 | `results/valgrind-*.log`、`cnt-*.tap`、`asan-*.tap` | W1 起沿用 `run-valgrind.sh` 與 `valgrind-dlopen.supp` |
| F8 | `client_id` 與 `connection_id` 在單一客戶端模式恆為 0，稽核寫「小於 65535」仍成立但不具檢驗力 | `O2-live.tap` 第 18、25 項 | 多客戶端情境在 W3 才有意義 |
| F9 | CEO 決定（使用者核准）`webui_set_context`、`webui_get_context` 改為範圍內 W2：W2=29、範圍內 104、排除 8（只剩 `webui_malloc`、`webui_memcpy` 與六個回呼或回應處理函式）。校驗器要求範圍內合計必須為 104。`webui_interface_get_context` 在 `include/webui.h` 中不存在（112 個原型沒有此名稱），因此函式清單沒有對應項目 | `functions.txt`、`results/functions.tap`、`results/protos.tap` | 若指的是別的名稱，請 CEO 確認 |
| F10 | 在瀏覽器模式、不顯示任何視窗時，`webui_new_window` 或 `webui_new_window_id` 仍會經 `webui_set_center` 呼叫 `_webui_load_gtk_and_webkit`，以 dlopen 載入 GTK、glib、WebKit；這些函式庫的建構函式配置了 2,065 位元組（definite）永不釋放，是 valgrind 看到的唯一 definite 洩漏 | `results/valgrind-raw.log` 的堆疊 | 屬 WebUI 依賴的第三方函式庫，不屬於 bridge；以抑制檔分開回報，W1 的 bridge 不需處理，但文件要註明 |
| F11 | `folder_monitor` 的效果在 W0 沒有觀察，規格修訂 3 已把它延到 W5 | 規格 7 的 W1 與 W5 列 | W5 實作並觀察 |

函式庫行為與稽核一致處：`set_root_folder`、`set_port`、`set_public`、`encode`、`decode`、`new_window_id`、`multi_client` 下 `script` 回假、`ui_event_blocking`、`asynchronous_response` 的資料有效期，皆符合稽核。

## 6. 未結項目與無法在本環境執行的項目

未結：

- E1 預期引數個數為 4（已實測並被規格採用）；稽核表本身仍寫 5
- `webui_script` 旗標加緩衝區行為已有實測（4 節 E2），W3 須照規則 12 實作並測試
- `folder_monitor` 的效果只在 W5 觀察
- 稽核表仍把 `webui_get_hwnd` 系列列為排除（F4），待 Architect 更新
- bridge 本體尚未存在，valgrind 與計數器對 bridge 的結果要到 W1 才有

本環境無法執行：


- 32 位元（`-m32`）：沒有 multilib，連結 `-m32` 時缺 `Scrt1.o`、`crti.o`，Win32 與 x32 重跑延後
- `ss`：不存在，改以讀取 `/proc/net/tcp` 與 `/proc/net/tcp6` 判斷監聽位址
- `hbmk2` 與 `harbour`：不存在，`bridge_request.prg` 只驗證了產生器輸出的行數（104 行 `REQUEST`）與命名規則，未經 Harbour 編譯，`.hrb` 解析檢查（規格 8.3）尚未做
- Windows 與 WebView 模式：未測
- valgrind 沒有跑 E 組（瀏覽器），只跑無頭群組
- clang：沒有跑矩陣（只有 gcc 13.3）

## 7. 交付檔案（`poc/harbour-webui/w0/`）

| 檔案 | 用途 |
|---|---|
| `functions.txt` | 112 筆函式清單：名稱、波次、領域，依標頭順序 |
| `gen_functions.py` | 檢查清單（標頭每個函式恰好一次、各波數量 12/29/27/25/11/8、範圍內合計 104、排除名單為 `webui_malloc` 等 8 個、W1 名單等於規格）並產生 `bridge_request.prg`（`--request`）、列出波次名單（`--names`） |
| `bridge_request.prg` | 產生的 104 行 `REQUEST WEBUI_*`（名稱為 C 名稱大寫，例如 `WEBUI_NEW_WINDOW`）；舊的 PoC-001 名稱 `WEBUI_NEWWINDOW`、`WEBUI_BINDHB` 未納入，W1 決定 |
| `hdrparse.py`、`check_protos.py` | 標頭剖析、A3 漂移檢查、A4 原型比對、產生 `abi_enums_gen.h` |
| `check_bool_abi.py` | B 組 |
| `bridge_decl.h`、`bridge_consts.h`、`bridge_num.h` | 手寫的 C89 宣告、列舉常數、`double` 與 `size_t` 轉換輔助，W1 的 bridge 可直接採用 |
| `abi_bridge.c`（C89）、`abi_real.c`（C99）、`abi_i64.c`（C99）、`abi_live.html` | 測試本體與 E 組頁面 |
| `Makefile`、`run-abi.sh` | 建置與矩陣執行，輸出 TAP |
| `abi_counters.c` | 配置計數器（cnt 變體，`--wrap`） |
| `run-valgrind.sh`、`vg_classify.py`、`valgrind-dlopen.supp` | valgrind 兩輪執行、洩漏分類、GTK 與 glib 的抑制檔 |
| `results/` | 最後一次完整執行的 TAP、摘要與 valgrind 日誌 |

重跑單一變體：`VARIANTS=O2 ./run-abi.sh`；略過 E 組：`NO_LIVE=1 ./run-abi.sh`；valgrind 快速版：`ITER=20 ./run-valgrind.sh`。

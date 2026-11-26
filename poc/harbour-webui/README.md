# PoC-001：Harbour + WebUI

依 [POC-SPEC.md](../../docs/team/POC-SPEC.md) 實作的最小可行原型。視窗顯示一個輸入框與按鈕，按鈕呼叫 Harbour 函式 `Hello()`，把回傳字串顯示在頁面上。

## 目錄

```
poc/harbour-webui/
├── prg/
│   ├── main.prg     主程式：建立視窗、綁定 hello、顯示頁面、等待關閉
│   └── hello.prg    Hello(cName)：回傳問候字串並計數
├── src/
│   └── bridge.c     C89 膠合層：WebUI <-> Harbour
├── ui/
│   └── index.html   介面（輸入框、按鈕、結果）
└── README.md
```

## 架構要點

- `bridge.c` 不引用 `webui.h` 與 `hbapi.h`。兩者都含有 C99 語法（`//` 註解、`long long`），無法通過 `-std=c89 -pedantic-errors`。bridge 改為自行宣告所用的 API 子集，只使用 `int`、`size_t` 與指標。
- `size_t` 與 Harbour 的 `HB_SIZE` 在 x32 Windows、x64 Windows、64 位元 Linux 上寬度相同。
- WebUI 的回呼在 WebUI 的執行緒中執行，並以 Harbour VM 呼叫 `Hello()`。PoC 只綁定一個處理函式。

## 驗證狀態

| 項目 | Linux（GCC 13.3、Harbour 3.2.0） | bcc_x32（BCC 7.4） | vs2019-x32 | vs2019-x64 |
|---|---|---|---|---|
| A1 bridge 通過 `-std=c89 -pedantic-errors` | 通過 | 待測 | 待測 | 待測 |
| A2 建置與連結 | 通過 | 待測 | 待測 | 待測 |
| A3 執行與互動 | 通過 | 待測 | 待測 | 待測 |
| A4 呼叫計數 1、2、3 | 通過 | 待測 | 待測 | 待測 |
| A5 關閉視窗後結束 | 通過 | 待測 | 待測 | 待測 |

Linux 驗證於 Xvfb 下進行：WebUI 啟動 Chrome，以 xdotool 點擊按鈕，連續三次呼叫後頁面顯示 `call #3`，之後關閉視窗，程式正常結束。

## 建置：Linux（GCC，驗證用）

以下指令在 `/workspace` 倉庫根目錄與 `poc/harbour-webui/` 下執行，需要已安裝 Harbour 3.2.0（`hbmk2` 在 PATH 中）。

```sh
# 1. 建置 WebUI 靜態函式庫
make                                   # 產出 dist/libwebui-2-static.a

# 2. 檢查 bridge 的 C89 合規性
cd poc/harbour-webui
gcc -std=c89 -pedantic-errors -Wall -Wextra -c src/bridge.c -o /tmp/bridge.o

# 3. 建置 PoC
hbmk2 prg/main.prg prg/hello.prg src/bridge.c \
  -I../../include -L../../dist -lwebui-2-static -lpthread -lm -ldl \
  -ohbwebui

# 4. 執行（需在 poc/harbour-webui 下，才能讀到 ui/index.html）
./hbwebui
```

## Windows 目錄配置

Windows 上的建置以並排的資料夾為準：

```
d:\hbdev32\
├── webui\              WebUI 函式庫（本倉庫），產出 dist\webui-2-static.lib
├── harbour-webui\      本 PoC 應用程式（由 poc/harbour-webui 複製而來）
├── harbour-core\       Harbour 原始碼與 bin\win\...
└── bcc_x32.bat, vs2019-x32.bat, vs2019-x64.bat
```

以下 Windows 指令假設 PoC 位於 `d:\hbdev32\harbour-webui\`，WebUI 函式庫位於 `d:\hbdev32\webui\`。

## 建置：Windows（bcc_x32，待測）

尚未驗證。步驟依 [ENVIRONMENT.md](../../docs/team/ENVIRONMENT.md) 整理，第一次測試時請回報實際結果，並校正下列指令。

```bat
rem 1. 在 bcc_x32 環境建置 WebUI 靜態函式庫（BCC 尚無對應 Makefile，需先確認 webui-2-static.lib 的建置方式）
cd d:\hbdev32
call bcc_x32.bat

rem 2. 建置 PoC
cd d:\hbdev32\harbour-webui
d:\hbdev32\harbour-core\bin\win\bcc\hbmk2.exe prg\main.prg prg\hello.prg src\bridge.c ^
  -I..\webui\include -L..\webui\dist -lwebui-2-static ^
  -lws2_32 -lole32 -luser32 -ladvapi32 -lshell32 ^
  -ohbwebui.exe

rem 3. 執行
hbwebui.exe
```

BCC 7.4 的預設 C 標準與 C89 不同（見 ENVIRONMENT.md 第 2.1 節），bridge 的 C89 合規性要以明確的編譯參數驗證。

## 建置：Windows（vs2019-x32，待測）

同樣尚未驗證。bcc_x32 失敗時改以此環境為主。

### 一鍵建置腳本

- 腳本：`scripts\build-vs2019-x32.bat`（CRLF 換行、純 ASCII）。
- 狀態：**UNVERIFIED on Windows**（撰寫時無法在 Windows 上執行，第一次執行請回報結果）。
- 前提：先以 nmake 建好 `d:\hbdev32\webui\dist\webui-2-static.lib`（見下方步驟 1）。
- 用法：開一個**全新乾淨**的命令提示字元（`vcvarsall.bat` 只能呼叫一次；若視窗已有 VS 環境，腳本會拒絕執行），然後執行：

```bat
cd /d d:\hbdev32\harbour-webui
scripts\build-vs2019-x32.bat
```

- 腳本會依序：設定 ENVIRONMENT.md 所列的 `HB_*` 變數與 `chcp 65001`、檢查 `hbmk2.exe` 與四個來源檔（`prg\main.prg`、`prg\hello.prg`、`src\bridge.c`、`ui\index.html`）、檢查 WebUI 靜態函式庫、呼叫一次 `vcvarsall.bat x86`、執行 hbmk2。成功時印出 `hbwebui.exe` 的完整路徑；失敗時結束碼為 1。
- 假設值（無法由雲端確認，若不符請改腳本頂端的設定）：`vcvarsall.bat` 位於 `C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise\VC\Auxiliary\Build\vcvarsall.bat`（可先設定環境變數 `VCVARSALL` 覆蓋）；OpenSSL、curl、ADS 路徑以 `d:\hbdev32\` 為根，依 ENVIRONMENT.md 的相對路徑組成。
- 記錄檔：`scripts\build-vs2019-x32.log`，同一份輸出也會在結束後顯示在主控台（不是即時輸出，hbmk2 執行期間主控台不會有畫面）。
- 失敗時：
  1. 開啟 `scripts\build-vs2019-x32.log`，搜尋 `[ERROR]`，最後一個 `[ERROR]` 行即為失敗的步驟（缺檔、缺 `webui-2-static.lib`、`vcvarsall.bat` 找不到、hbmk2 回傳非零）。
  2. 若是 hbmk2 失敗，其編譯器與連結器的訊息在 `Running hbmk2 ...` 之後。
  3. 回報時請附上整份記錄檔。

以下為不使用腳本時的手動步驟：

```bat
rem 1. 在 vs2019-x32 環境建置 WebUI 靜態函式庫（倉庫根目錄 Makefile 為 nmake 版本，產出 dist\webui-2-static.lib）
cd d:\hbdev32\webui
call d:\hbdev32\vs2019-x32.bat
nmake

rem 2. 建置 PoC
cd d:\hbdev32\harbour-webui
d:\hbdev32\harbour-core\bin\win\msvc\hbmk2.exe prg\main.prg prg\hello.prg src\bridge.c ^
  -I..\webui\include -L..\webui\dist -lwebui-2-static ^
  -lws2_32 -lole32 -luser32 -ladvapi32 -lshell32 ^
  -ohbwebui.exe

rem 3. 執行
hbwebui.exe
```

注意：`vcvarsall.bat` 只能呼叫一次（見 ENVIRONMENT.md 第 2.1 節）。若 `call` 之後仍找不到 `cl`，請開新的命令提示字元，再執行上述步驟。

vs2019-x32 的 `hbmk2.exe` 問題已由使用者確認解決。若重新出現問題，請先比對 `bin\win\msvc\hbmk2.exe` 與原先版本。

## 已知限制

- PoC 只支援單一綁定函式（`bridge.c` 中的 `s_szHandler`）。
- 處理函式在 WebUI 執行緒中呼叫 Harbour VM。Linux 驗證無問題，但 Windows 上尚未驗證，也未確認 Harbour 是否為多執行緒建置。若出現異常，改為由主執行緒處理佇列。
- 頁面需從 PoC 所在的資料夾下啟動，才能讀到 `ui/index.html`。

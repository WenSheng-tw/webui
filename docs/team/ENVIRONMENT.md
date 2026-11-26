# 建置環境

本文件記錄使用者電腦（Windows）上的建置環境，依據 `D:\hbdev32\` 下的三個批次檔整理。

## 1. 共同設定

- Harbour 原始碼：`d:\hbdev32\harbour-core`，由原始碼自行編譯。
- 建置模式：`HB_BUILD_MODE=c`（Harbour 先產生 C 碼，再以原生 C 編譯器編譯）。
- 只產生靜態函式庫：`HB_BUILD_DYN=no`、`HB_BUILD_CONTRIB_DYN=no`。
- 不編譯 contrib：`HB_BUILD_CONTRIBS` 為空。
- OpenSSL 與 curl 皆為靜態連結（`HB_STATIC_OPENSSL=yes`、`HB_STATIC_CURL=yes`）。

## 2. 三種環境

| 項目 | bcc_x32 | vs2019-x32 | vs2019-x64 |
|---|---|---|---|
| 批次檔 | `bcc_x32.bat` | `vs2019-x32.bat` | `vs2019-x64.bat` |
| 編譯器 | Borland C++ 7.4（`d:\bcc74`） | VS2019 Enterprise，`vcvarsall x86` | VS2019 Enterprise，`vcvarsall x86_amd64` |
| `HB_COMPILER` | `bcc` | `msvc` | `msvc64` |
| Harbour 工具路徑 | `d:\hbdev32\harbour-core\bin\win\bcc` | `d:\hbdev32\harbour-core\bin\win\msvc` | `d:\hbdev32\harbour-core\bin\win\msvc64` |
| 字元集 | 未指定 | `/source-charset:utf-8 /execution-charset:utf-8`，`chcp 65001` | 同 vs2019-x32 |
| `HB_USER_CFLAGS` | `-O2 -Id:\bcc74\include\windows\crtl -Id:\bcc74\include\windows\sdk` | `/source-charset:utf-8 /execution-charset:utf-8` | 同 vs2019-x32 |
| `HB_USER_PRGFLAGS` | 未指定 | `-l-` | `-l-` |
| `HB_BUILD_OPTIM` | `no` | 未指定 | 未指定 |
| `HB_BUILD_WARN` / `HB_TRACE` | `yes` / `yes` | 未指定 | 未指定 |
| OpenSSL | 3.5.9，`openssl-3.5.9\x32` | 3.5.9，`openssl-3.5.9\x32` | 3.5.9，`openssl-3.5.9\x64` |
| curl | 8.23.0，`curl-8.23.0\release\x32` | 8.23.0，`curl-8.23.0\release\x32` | 8.23.0，`curl-8.23.0\release\x64` |
| ADS | `acesdk\x32` | `acesdk\x32` | `acesdk\x64` |

### 2.1 注意事項

- **bcc_x32**：批次檔註解指出，BCC 7.1 x64 的 `bin\bcc64.cfg` 預設加上 `-std=c11`。這表示編譯器預設標準不是 C89，C89 必須由我們在編譯參數中明確指定。
- **vs2019**：`vcvarsall.bat` 只能呼叫一次，多次呼叫會造成路徑名稱過長。
- **vs2019**：原先 VS2019 編出的 `hbmk2.exe` 有問題，使用者已確認此問題解決。
## 3. MySQL

- 版本：MySQL 5.0（WinApp）。
- 路徑：`D:\hbdev32\mod_harbour\mod_harbour.v2\ssbbs\` 目錄下的 `mysql50`。
- 該目錄下還有其他套件，之後可能會用到。
- 整合 MySQL 不在第一階段，目前不納入建置設定。

## 4. WebUI 在 Windows 上的編譯狀態

- 目前尚未驗證。倉庫的 `GNUmakefile` 是給 GCC 使用的，MSVC 有 `Makefile`（nmake），BCC 7.4 尚無對應建置檔。
- 第一步：在 bcc_x32 測試編譯 WebUI 靜態函式庫（`webui-2-static.lib`）。
- 若 bcc_x32 無法完成，改以 vs2019-x32 為主要環境。

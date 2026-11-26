# PoC-001 測試結案報告（Linux）

- 撰寫：CEO Agent（依據 QA Agent 的驗收報告）
- 驗收對象：分支 `cursor/poc-harbour-webui-a86d`，commit `775ac56`
- 驗收環境：Ubuntu、GCC 13.3、Harbour 3.2.0、Xvfb 加 Chrome、xdotool
- 結論：**Linux 驗收通過（A1 至 A5 全部 Pass）。Windows 項目全部尚未驗證，已依使用者決定延後。**

## 1. 驗收結果

| 項目 | 結果 | 說明 |
|---|---|---|
| A1 C89 合規 | Pass | `gcc -std=c89 -pedantic-errors -Wall -Wextra -c src/bridge.c` 回傳 0，無警告。`nm` 顯示只有 bridge 宣告的外部符號 |
| A2 建置 | Pass | 以 `make` 建出 WebUI 靜態庫，再以 hbmk2 連結出 `hbwebui`。README 的 Linux 步驟可原樣重現 |
| A3 視窗與互動 | Pass | 標題為「Harbour + WebUI PoC-001」；點擊顯示 `Harbour says: Hello, World! (call #1)`。中文輸入與空輸入皆正確 |
| A4 連續呼叫 | Pass | 計數依序為 1、2、3，之後持續累加。快速連按 1200 次，計數停在 1200，程序未當機 |
| A5 正常結束 | Pass | 關閉視窗後約 2.4 秒內結束，結束碼 0 |
| 缺少 `ui/index.html` | Pass | 顯示錯誤訊息並等待按鍵，按鍵後結束碼為 0 |

## 2. 測試期間發現並修正的缺陷

| 嚴重度 | 缺陷 | 處理 |
|---|---|---|
| 主要 | WebUI 為每個回呼建立新執行緒，Harbour VM 只有單一堆疊。快速連按會造成 `Unrecoverable error` 並當機 | 修正：在綁定前以 `webui_set_config` 開啟全域的 `ui_event_blocking`（commit `28deebe`）。修正前後均已在真實應用程式上實測 |
| 次要 | 綁定不存在的處理函式時仍回傳綁定編號，到點擊時才出錯 | 修正：綁定時以 `hb_dynsymIsFunction` 檢查，找不到回傳 0，且失敗的綁定不覆蓋先前的處理函式（`848088f`、`48fb38b`） |
| 次要 | 缺少 `index.html` 時視窗一閃即逝 | 修正：顯示訊息後等待按鍵（`6041097`） |

## 3. 腳本審查（靜態，未執行）

- `scripts/build-vs2019-x32.bat`：QA 審查通過，並依審查加入前置檢查與明確的 `-st`。
- `scripts/probe-bcc-dll.bat`：QA 第一次審查要求修正（避免覆蓋 MSVC 版執行檔、清除舊檔、補充 Ctrl+C 說明），修正後複審通過。

## 4. 尚未驗證的項目（僅 Windows）

- vs2019-x32：以 `nmake` 建置 WebUI 靜態庫、hbmk2 連結、建置腳本實際執行。
- bcc_x32：以 WebUI DLL 加 `implib` 的路線、探測腳本實際執行。
- BCC 與 MSVC 對 `bridge.c` 的 C89 編譯檢查。
- Windows 上的 WebUI 執行緒行為、WebView2 或瀏覽器啟動、系統函式庫清單、UTF-8 與主控台。

## 5. 已知限制

- bridge 目前只支援單一處理函式表，第二次 `WEBUI_BINDHB` 會取代處理函式，屬 PoC 範圍內的限制。
- `webui_show` 的回傳值未回報給 Harbour，瀏覽器啟動失敗時沒有訊息。
- 目前以單執行緒 Harbour VM 為前提。若改用多執行緒 VM，需要另行處理 WebUI 執行緒的 VM 初始化。

## 6. 後續

- Windows 驗證延後，待使用者環境可用時依 `scripts` 內的腳本進行。
- 下一階段為 PoC-002（WebUI 完整 API 的 C89 綁定、記憶體回收與雙模式測試），規格已提交使用者核准。

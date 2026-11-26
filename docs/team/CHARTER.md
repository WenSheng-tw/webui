# 網緹 AI 團隊章程

## 1. 專案目標

以 **標準 C89** 撰寫程式，以 **Harbour** 為後端邏輯，以 **WebUI** 作為 Harbour 的圖形介面（UI），開發 WinApp。WebApp 另案規劃。

## 2. 組織與回報

團隊共 9 位成員，回報鏈如下，對外只有使用者一位上級。

```
使用者（上級）
  └─ CEO Agent：交辦、盯進度、把關 QA 結果、撰寫對外文件；不寫程式
       └─ PM Agent：拆工作、排時程、大型實作前出規格供使用者核准
            ├─ Architect Agent：WinApp / WebApp 底層架構與技術做法
            ├─ Back-end Agent：伺服器端邏輯、API、報表
            ├─ Front-end Agent：WebApp 瀏覽器端互動與資料呈現
            ├─ Design Agent：版面、操作動線、CSS
            ├─ DBA Agent：資料表、SQL 效能、資料清理、ERD、資料字典
            ├─ QA Agent：程式碼審查、功能與邊界測試、畫面邏輯檢查
            └─ Ops Agent：環境建置、自動化腳本、部署支援
```

## 3. 角色職責

| 角色 | 回報對象 | 主要職責 | 主要產出 |
|---|---|---|---|
| CEO | 使用者 | 接收需求、交辦 PM、驗收 QA 結果 | 發布說明、操作手冊、客戶提案、測試結案報告 |
| PM | CEO | 工作拆解、排程、進度追蹤、規格送審 | 一頁規格、進度報告 |
| Architect | PM | WinApp / WebApp 架構與技術選型 | 架構文件、技術決策紀錄 |
| Back-end | PM | 伺服器邏輯、API、報表（WinApp：Harbour、FiveWin 或 WebUI；WebApp：PHP 或 mod_harbour） | 程式碼、API 文件 |
| Front-end | PM | WebApp 瀏覽器端互動與資料呈現（jQuery、Bootstrap、w2ui） | 程式碼 |
| Design | PM | 版面、操作動線、CSS | CSS 檔（第一行必須為 `@charset 'UTF-8';`） |
| DBA | PM | 資料表設計、SQL 效能、清理、一致性；ERD 與資料字典 | 資料表結構、ERD、資料字典 |
| QA | PM | 程式碼審查、功能與邊界測試、畫面邏輯檢查 | 測試報告、缺陷清單 |
| Ops | PM | 環境建置、自動化腳本、部署 | 建置腳本、部署說明 |

## 4. 工作流程

1. 使用者向 CEO 提出需求，CEO 交辦 PM。
2. PM 請 Architect 定架構，並出一頁規格送使用者核准。
3. 核准後，PM 將工作拆給 Back-end、Front-end、Design、DBA。
4. QA 審查與測試，未通過者退回原角色修正。
5. QA 通過後，CEO 彙整成對外文件，回報使用者。
6. Ops 負責建置與部署。

## 5. 技術規範

### 5.1 C89
- 團隊自寫的 C 程式碼必須能以 `-std=c89 -pedantic-errors` 通過編譯，並在每個目標編譯器上驗證（BCC 7.4、MSVC VS2019）。
- 禁止 `//` 註解、C99 型別（`bool`、`stdint.h`）、在區塊中間宣告變數、VLA 等 C99 以後的語法。
- 第三方函式庫（例如 WebUI 原始碼）不屬於 C89 合規範圍，但其標頭檔不得直接被團隊的 C89 程式碼引用。改由團隊自行宣告所需的 API，集中在隔離層（bridge）。
- 不依賴編譯器的預設 C 標準。編譯參數必須明確指定。

### 5.2 字元集
- 原始碼一律 UTF-8。
- MSVC 必須加上 `/source-charset:utf-8 /execution-charset:utf-8`，並在建置前執行 `chcp 65001`。

### 5.3 CSS
- 所有 CSS 檔案第一行必須是 `@charset 'UTF-8';`。

### 5.4 資料庫
- WinApp：MySQL 5.0，使用 `D:\hbdev32\mod_harbour\mod_harbour.v2\ssbbs\` 目錄下的 `mysql50`，該目錄下的其他套件之後可能也會用到。（MySQL 整合目前不是第一階段重點。）
- WebApp：MySQL 5.7。

### 5.5 建置環境
詳見 [ENVIRONMENT.md](ENVIRONMENT.md)。

## 6. 版本控制

- 每個邏輯變更使用獨立 commit，commit 訊息要描述變更內容。
- 不做 force push，除非是自己擁有的 PR 分支，且已整合遠端的 commit。
- 不直接在 `main` 分支上開發，所有變更透過 PR 合併。

## 7. 決策紀錄

| 日期 | 決策 | 決定者 |
|---|---|---|
| 2026-11-26 | WinApp 的 UI 由 WebUI 取代 FiveWin | 使用者 |
| 2026-11-26 | 第一個 PoC 先以 bcc_x32 測試，若無法完成，改以 vs2019 為主 | 使用者與團隊 |
| 2026-11-26 | MySQL 整合列為後續工作，不在第一階段 | 使用者 |
| 2026-11-26 | vs2019 的 hbmk2.exe 問題已解決 | 使用者確認 |
| 2026-11-26 | WebUI 標頭檔不符合 C89，採隔離層（bridge）處理 | Architect（待使用者核准 PoC 規格） |
| 2026-11-26 | bcc_x32 改以 WebUI DLL 加匯入庫（PoC-001 修訂 1） | 使用者 |
| 2026-11-26 | 核准 PoC-002：WebUI 完整 API 的 C89 綁定，分波交付，雙模式測試，Windows 延後，以 Linux 為準 | 使用者 |

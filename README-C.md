# WebUI 運作原理解析

**WebUI** 的核心運作原理是：**利用作業系統中已安裝的現代網頁瀏覽器（或原生 WebView）作為前端 GUI 視窗，並透過內建的輕量級本地 HTTP 與雙向二進位 WebSocket 伺服器，與後端原生程式進行即時通訊。**

與 Electron（內嵌整個 Chromium 與 Node.js）或 Tauri（依賴作業系統內建的 WebView2 / WebKit 渲染引擎與複雜的橋接層）不同，WebUI 本身**不包含任何網頁渲染引擎**，這使得編譯後的執行檔極小且幾乎沒有額外執行期依賴。

---

## 一、前後端是如何連起來的？（核心通訊原理）

![WebUI 核心運作原理](docs/images/webui_architecture.png)

你可以把 WebUI 想像成**「在自己電腦上開一個只有你自己看得到的迷你網站」**：

1. **後端原生程式（你的程式本體）：**
   當後端程式（如 C / C++ / Python / Go 等）呼叫 `webui_new_window()` 並執行 `webui_show()` 時，WebUI 函式庫會在背景啟動一個綁定於本地迴路（`127.0.0.1`）隨機閒置連接埠的輕量級伺服器，僅佔用數十 KB。
2. **系統現有網頁瀏覽器（畫面視窗）：**
   WebUI 會自動偵測系統上已安裝的瀏覽器（如 Chrome、Edge、Firefox、Brave 等），並以**無工具列、無網址列的應用程式模式（App Mode）**啟動瀏覽器，同時建立**獨立的私人設定檔（Private Profile）**，讓它看起來像原生桌面軟體且不干擾日常上網的分頁與快取。
3. **雙向即時通訊（高速二進位 WebSocket 專線）：**
   網頁載入時會自動引入 `<script src="webui.js"></script>`，在後端程式與瀏覽器視窗之間建立一條**高速二進位 WebSocket 連線**：
   * **前端呼叫後端：** 使用者在畫面上操作（如點擊按鈕），`webui.js` 會透過 WebSocket 呼叫後端綁定（`webui_bind()`）的原生函數並取得回傳結果。
   * **後端操作前端：** 後端可隨時透過 `webui_run()` 或 `webui_script()` 推送 JavaScript 指令到瀏覽器端修改 DOM 或更新畫面。

---

## 二、為什麼它比 Electron 輕這麼多？（打包體積差異）

![桌面程式架構對比：Electron vs. WebUI](docs/images/webui_vs_electron.png)

* **傳統 Electron 架構（左邊）：**
  為了確保每台電腦都能顯示網頁介面，它把**「一整套 Chromium 瀏覽器核心 + Node.js」直接塞進安裝檔裡**。就算只寫一個簡單的「Hello World」視窗，打包體積也是 100MB 起跳。
* **WebUI 輕量架構（右邊）：**
  直接借用使用者電腦裡原本就裝好的 Chrome、Edge 或 Firefox 來渲染介面。執行檔內完全不包含瀏覽器引擎，只需連結僅**數十 KB** 的 WebUI 函式庫即可運行，兼具極致輕巧與現代 HTML5/CSS3/JS 完整功能。

---

## 三、常見架構問題（遠端連線與 HTTPS）

### 1. 支援遠端使用瀏覽器執行嗎？
**支援**，但預設基於安全考量僅綁定 `127.0.0.1`。若要開放區網或遠端連入，需進行以下設定：
* **開放外部連線：** 呼叫 `webui_set_public(my_window, true)` 監聽 `0.0.0.0`。
* **指定固定 Port：** 呼叫 `webui_set_port(my_window, 8080)` 固定連接埠。
* **無頭背景啟動：** 改用 `webui_show_browser(my_window, "index.html", NoBrowser)`，等待遠端瀏覽器輸入 `http://主機IP:8080` 連線。

### 2. 支援 HTTPS 連線嗎？
**支援**，兩種實作方式：
* **原生編譯 TLS 版本：** 編譯 WebUI 時加入 `WEBUI_USE_TLS=1` 連結 OpenSSL，並於程式中呼叫 `webui_set_tls_certificate()` 載入 PEM 憑證與私鑰。
* **透過反向代理（Reverse Proxy，推薦）：** 維持 WebUI 本身在 `127.0.0.1` 跑純 HTTP/WS，前端由 **Apache** 或 **Nginx** 掛載 SSL 憑證處理 HTTPS（Port 443），再將 HTTP 與 WebSocket 流量反向代理轉發至本機 WebUI 連接埠。

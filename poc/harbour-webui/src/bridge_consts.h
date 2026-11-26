/*
 * bridge_consts.h  --  hand-written copies of the WebUI enum values the bridge relies on
 * (ISO C89, macros only, safe to include next to the real webui.h).
 *
 * w0/check_protos.py compares every enumerator of include/webui.h with these macros
 * (static drift check) and abi_real.c compares them at run time (group A3).
 */

#ifndef BRIDGE_CONSTS_H
#define BRIDGE_CONSTS_H

/* enum webui_config (webui_set_config option) */
#define BRIDGE_CFG_SHOW_WAIT_CONNECTION 0
#define BRIDGE_CFG_UI_EVENT_BLOCKING 1
#define BRIDGE_CFG_FOLDER_MONITOR 2
#define BRIDGE_CFG_MULTI_CLIENT 3
#define BRIDGE_CFG_USE_COOKIES 4
#define BRIDGE_CFG_ASYNCHRONOUS_RESPONSE 5

/* enum webui_browser (size_t in the API) */
#define BRIDGE_BROWSER_NOBROWSER 0
#define BRIDGE_BROWSER_ANYBROWSER 1
#define BRIDGE_BROWSER_CHROME 2
#define BRIDGE_BROWSER_FIREFOX 3
#define BRIDGE_BROWSER_EDGE 4
#define BRIDGE_BROWSER_SAFARI 5
#define BRIDGE_BROWSER_CHROMIUM 6
#define BRIDGE_BROWSER_OPERA 7
#define BRIDGE_BROWSER_BRAVE 8
#define BRIDGE_BROWSER_VIVALDI 9
#define BRIDGE_BROWSER_EPIC 10
#define BRIDGE_BROWSER_YANDEX 11
#define BRIDGE_BROWSER_CHROMIUMBASED 12
#define BRIDGE_BROWSER_WEBVIEW 13

/* enum webui_runtime (size_t in the API) */
#define BRIDGE_RUNTIME_NONE 0
#define BRIDGE_RUNTIME_DENO 1
#define BRIDGE_RUNTIME_NODEJS 2
#define BRIDGE_RUNTIME_BUN 3

/* enum webui_event (event_type field) */
#define BRIDGE_EVENT_DISCONNECTED 0
#define BRIDGE_EVENT_CONNECTED 1
#define BRIDGE_EVENT_MOUSE_CLICK 2
#define BRIDGE_EVENT_NAVIGATION 3
#define BRIDGE_EVENT_CALLBACK 4

/* enum webui_logger_level */
#define BRIDGE_LOGGER_DEBUG 0
#define BRIDGE_LOGGER_INFO 1
#define BRIDGE_LOGGER_ERROR 2

#endif

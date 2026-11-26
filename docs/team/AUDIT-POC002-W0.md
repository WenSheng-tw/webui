# W0 / T0.1 AUDIT - WebUI public API (include/webui.h vs src/webui.c)

Basis: include/webui.h (112 WEBUI_EXPORT prototypes, 2.5.0-beta.4), src/webui.c read-only. Line = definition line in src/webui.c.

## 0. Scope correction

Scope (final, after spec revisions 1 and 2 and the set_context decision): the first CEO list named 12 functions, not 11 (set_context, get_context, webui_malloc, webui_memcpy, get_hwnd, win32_get_hwnd, set_logger, set_close_handler_wv, set_file_handler, set_file_handler_window, return_http, interface_set_response_file_handler); all 12 exist in the header. `webui_get_hwnd` and `webui_win32_get_hwnd` were then moved **into scope (W4)** by spec revision 1 (opaque OS handle, not heap memory, never freed). `webui_set_context` and `webui_get_context` were then moved **into scope (W2)** by CEO decision (bridge-side slot table, see rows 5 and 6; `webui_set_context` calls `webui_bind(window, element, NULL)`). **Excluded = 8, in scope = 112 - 8 = 104.**

Per wave (in scope): W1=12, W2=29, W3=27, W4=25, W5=11 (sum 104); excluded 8; header total 112.

Note: only `webui_set_config` takes an enum-typed parameter (`webui_config`, int). Browser/runtime/event enums are passed as `size_t` in the API (show_browser, browser_exist, set_runtime, get_best_browser return). `webui_event_t` pointer parameters appear in 22 in-scope functions: bridge must either mirror the struct (8 x size_t/ptr, layout test in T0.2-F) or use the `webui_interface_*` twins that need only (size_t window, size_t event_number).

Legend: ret = `scalar` no pointer; `WebUI-owned` do not free, copy at once; `caller-owned` free with `webui_free`. retain = `-` nothing retained after return; `FP` function pointer kept.

## 1. Full table

| # | function | (a) C signature | line | (e) area | (f) wave | (b) retains | (c) return owner | (d) bridge handling | thread-safety |
|---|---|---|---|---|---|---|---|---|---|
| 1 | webui_new_window | `size_t webui_new_window(void)` | 1334 | window | W1 | - | scalar | result 0 = failure (exit/id exhausted) -> raise/return 0 | safe; non-blocking (spawns server thread) |
| 2 | webui_new_window_id | `size_t webui_new_window_id(size_t window_number)` | 1344 | window | W3 | - | scalar | num 1..65534; destroys existing window with same id first | safe; destroy of live window blocks (see destroy) |
| 3 | webui_get_new_window_id | `size_t webui_get_new_window_id(void)` | 1412 | window | W3 | - | scalar | reserves id; 0 = none free (non-fatal path calls WEBUI_ASSERT -> abort risk when exhausted) | safe |
| 4 | webui_bind | `size_t webui_bind(size_t window, const char* element, void (*func)(webui_event_t* e))` | 2552 | bind/script | W1 | FP: func ptr kept until window destroy/clean (element string copied) | scalar | element copied by WebUI; bridge passes static trampoline; result bind id | safe; do not call from inside callback of same window while destroying |
| 5 | webui_set_context | `void webui_set_context(size_t window, const char* element, void* context)` | 2523 | bind/script | W2 | context pointer kept (webui.c:2545 `cb_context[cb_index]`); **side effect: calls `webui_bind(window, element, NULL)` (webui.c:2542)**, registering the element with no handler | void | bridge-side slot table: copy the Harbour value into bridge storage, pass the slot number as `void*` (never a Harbour pointer); replacing or clearing an element's context releases the old slot; all slots released after `webui_clean` (not in callback); bind side effect means the element appears bound, document it | safe; same locking as bind |
| 6 | webui_get_context | `void* webui_get_context(webui_event_t* e)` | 2499 | bind/script | W2 | - | opaque slot number (`void*` set by set_context; NULL if none/event dead; never free) | needs event struct: rebuild from (window, event_number, element) or look up by bind id; map slot number back to the stored Harbour value (NIL for NULL or an unknown slot) | safe in callback; reads `cb_context[cb_index]` of the element |
| 7 | webui_get_best_browser | `size_t webui_get_best_browser(size_t window)` | 2622 | show | W3 | - | scalar | size_t browser id | safe |
| 8 | webui_show | `bool webui_show(size_t window, const char* content)` | 2433 | show | W1 | - | scalar | copy content with hb_parc not needed (WebUI copies); return hb_retl | BLOCKS up to startup_timeout (default 15 s) waiting for window connection; must be main thread in WebView mode; hazard in callback (blocks callback thread) |
| 9 | webui_show_client | `bool webui_show_client(webui_event_t* e, const char* content)` | 2296 | show | W3 | - | scalar | needs webui_event_t* -> bridge rebuilds struct from (window,event_number,connection_id) or use interface_show_client | callback-only; blocks like show |
| 10 | webui_show_browser | `bool webui_show_browser(size_t window, const char* content, size_t browser)` | 2477 | show | W3 | - | scalar | size_t browser = webui_browser value 0..13 (not enum-typed) | BLOCKS like show |
| 11 | webui_start_server | `const char* webui_start_server(size_t window, const char* content)` | 2255 | show | W3 | - | WebUI-owned (win->url or literal "") | copy with hb_retc; side effect: webui_set_timeout(0) (global!) (webui.c:2277) | BLOCKS/not; sets global timeout 0 |
| 12 | webui_show_wv | `bool webui_show_wv(size_t window, const char* content)` | 2455 | show | W3 | - | scalar | needs GTK/WebKit (dlopen) on Linux; false if unavailable; main thread | BLOCKS like show; pumps GTK |
| 13 | webui_set_kiosk | `void webui_set_kiosk(size_t window, bool status)` | 1438 | window | W4 | - | void | bool param passed as int slot | safe |
| 14 | webui_focus | `void webui_focus(size_t window)` | 2381 | window | W4 | - | void | window id | safe; no-op on Linux |
| 15 | webui_set_custom_parameters | `void webui_set_custom_parameters(size_t window, char *params)` | 1455 | window | W4 | - | void | char* (non-const) params; copied; hb_parc cast ok | safe |
| 16 | webui_set_high_contrast | `void webui_set_high_contrast(size_t window, bool status)` | 1501 | window | W4 | - | void | bool param passed as int slot | safe |
| 17 | webui_set_resizable | `void webui_set_resizable(size_t window, bool status)` | 1484 | window | W4 | - | void | bool param passed as int slot | safe |
| 18 | webui_is_high_contrast | `bool webui_is_high_contrast(void)` | 1527 | window | W4 | - | bool | runs popen(gsettings) on Linux | BLOCKS briefly (spawns process) |
| 19 | webui_browser_exist | `bool webui_browser_exist(size_t browser)` | 1518 | show | W3 | - | bool | size_t browser id; probes filesystem | safe; spawns/reads fs |
| 20 | webui_wait | `void webui_wait(void)` | 4552 | wait/close | W1 | - | void | release Harbour VM lock/scheduler if MT; never in callback | BLOCKS until all windows closed or webui_exit; WebView/Linux runs gtk_main on caller thread (main thread only); hazard in callback |
| 21 | webui_wait_async | `bool webui_wait_async(void)` | 4561 | wait/close | W1 | - | bool | poll loop in Harbour: DO WHILE webui_wait_async(); hb_idleState; ENDDO | non-blocking (one GTK pump); final call sleeps ~850 ms in wait_clean; main thread only |
| 22 | webui_close | `void webui_close(size_t window)` | 1616 | wait/close | W1 | - | void | non-blocking | safe in callback (sends CLOSE / flags WebView stop) |
| 23 | webui_minimize | `void webui_minimize(size_t window)` | 3542 | window | W4 | - | void | window id | safe |
| 24 | webui_maximize | `void webui_maximize(size_t window)` | 3564 | window | W4 | - | void | window id | safe |
| 25 | webui_close_client | `void webui_close_client(webui_event_t* e)` | 1580 | wait/close | W3 | - | void | event struct (or interface_close_client) | callback-only; closes one client connection |
| 26 | webui_destroy | `void webui_destroy(size_t window)` | 1661 | window | W4 | - | void | call from non-callback thread; frees window; ids reusable | outside callback BLOCKS up to ~4 s (2.5 s + 1.5 s); inside callback of that window only flags destroy_requested and returns (webui.c:1696-1701) |
| 27 | webui_exit | `void webui_exit(void)` | 4192 | wait/close | W1 | - | void | safe cleanup trigger; call from callback allowed | BLOCKS up to ~2 s (4x500 ms) even inside callback (own thread counts as running); idempotent |
| 28 | webui_set_root_folder | `bool webui_set_root_folder(size_t window, const char* path)` | 4627 | folders | W5 | - | scalar | path copied into fixed buffer WEBUI_MAX_PATH; false if folder missing (and resets to default) | safe |
| 29 | webui_set_browser_folder | `void webui_set_browser_folder(const char* path)` | 4607 | folders | W5 | - | void | path duplicated (tracked), replaced on next call | safe |
| 30 | webui_set_default_root_folder | `bool webui_set_default_root_folder(const char* path)` | 4673 | folders | W5 | - | scalar | copied; applies to all existing windows | safe |
| 31 | webui_set_close_handler_wv | `void webui_set_close_handler_wv(size_t window, bool (*close_handler)(size_t window))` | 1059 | OUT | OUT | - | - | - | excluded by CEO list |
| 32 | webui_set_file_handler | `void webui_set_file_handler(size_t window, const void* (*handler)(const char* filename, int* length))` | 1077 | OUT | OUT | - | - | - | excluded by CEO list |
| 33 | webui_set_file_handler_window | `void webui_set_file_handler_window(size_t window, const void* (*handler)(size_t window, const char* filename, int* length))` | 1101 | OUT | OUT | - | - | - | excluded by CEO list |
| 34 | webui_interface_set_response_file_handler | `void webui_interface_set_response_file_handler(size_t window, const void* response, int length)` | 4956 | OUT | OUT | - | - | - | excluded by CEO list |
| 35 | webui_is_shown | `bool webui_is_shown(size_t window)` | 1754 | show | W3 | - | bool | poll | safe |
| 36 | webui_set_timeout | `void webui_set_timeout(size_t second)` | 4570 | wait/close | W1 | - | void | size_t seconds: clamp 0..60 in bridge (negative double -> huge size_t) | safe; 0 = wait forever |
| 37 | webui_set_icon | `void webui_set_icon(size_t window, const char* icon, const char* icon_type)` | 1771 | window | W4 | - | void | icon + icon_type copied (webui.c:1794-1810); both non-empty else ignored | safe |
| 38 | webui_set_icon_file | `void webui_set_icon_file(size_t window, const char* path)` | 1844 | window | W4 | - | void | path read and file contents copied into WebUI buffer (webui.c:1894-1907) | safe; file I/O |
| 39 | webui_encode | `char* webui_encode(const char* str)` | 4065 | strings/returns | W2 | - | caller-owned: webui_free | NULL for empty/exiting/failure; hb_retc then webui_free(ptr) | safe; no-op/NULL after exit |
| 40 | webui_decode | `char* webui_decode(const char* str)` | 4107 | strings/returns | W2 | - | caller-owned: webui_free | as encode; decoded bytes NUL-terminated, binary length lost (use hb_retc only for text) | safe; no-op/NULL after exit |
| 41 | webui_free | `void webui_free(void* ptr)` | 4149 | strings/returns | W2 | - | void | only for encode/decode results; untracked ptr ignored; NO-OP after webui_exit (memory reclaimed by webui_clean) | safe; linear scan of ptr list |
| 42 | webui_malloc | `void* webui_malloc(size_t size)` | 4178 | OUT | OUT | - | - | - | excluded by CEO list |
| 43 | webui_memcpy | `void webui_memcpy(void* dest, void* src, size_t count)` | 4163 | OUT | OUT | - | - | - | excluded by CEO list |
| 44 | webui_send_raw | `void webui_send_raw(size_t window, const char* function, const void* raw, size_t size)` | 4018 | bind/script | W3 | - | void | raw buffer copied into packet (webui.c:4036-4062); binary-safe via hb_parclen | safe; non-blocking |
| 45 | webui_send_raw_client | `void webui_send_raw_client(webui_event_t* e, const char* function, const void* raw, size_t size)` | 3965 | bind/script | W3 | - | void | event struct (or interface_send_raw_client) | callback-only |
| 46 | webui_set_hide | `void webui_set_hide(size_t window, bool status)` | 3403 | window | W4 | - | void | bool param passed as int slot | safe |
| 47 | webui_set_size | `void webui_set_size(size_t window, unsigned int width, unsigned int height)` | 3586 | window | W4 | - | void | unsigned int w,h; validated 100..3840 x 100..2160, else ignored (size_set=false); bridge validates range first | safe; may run JS resizeTo (webui_run) |
| 48 | webui_set_minimum_size | `void webui_set_minimum_size(size_t window, unsigned int width, unsigned int height)` | 3643 | window | W4 | - | void | unsigned int; same range check | safe |
| 49 | webui_set_position | `void webui_set_position(size_t window, unsigned int x, unsigned int y)` | 3669 | window | W4 | - | void | unsigned int x,y; range 0..3000 x 0..1800 else ignored | safe |
| 50 | webui_set_center | `void webui_set_center(size_t window)` | 3732 | window | W4 | - | void | window id | safe; GTK/display probe on Linux |
| 51 | webui_set_profile | `void webui_set_profile(size_t window, const char* name, const char* path)` | 3803 | window | W4 | - | void | name + path duplicated; may create folder | safe; fs |
| 52 | webui_set_proxy | `void webui_set_proxy(size_t window, const char* proxy_server)` | 3849 | window | W4 | - | void | copied | safe |
| 53 | webui_get_url | `const char* webui_get_url(size_t window)` | 3902 | network/port | W3 | - | WebUI-owned (win->public_url) | copy immediately: freed/replaced on next get_url of same window; may trigger implicit show(NoBrowser,wait_connection) -> blocking | BLOCKS potentially (starts server) |
| 54 | webui_open_url | `void webui_open_url(const char* url)` | 3890 | show | W3 | - | void | url used immediately (launches default browser) | safe; spawns process |
| 55 | webui_set_public | `void webui_set_public(size_t window, bool status)` | 3941 | network/port | W5 | - | void | bool; reloads running server | safe |
| 56 | webui_navigate | `void webui_navigate(size_t window, const char* url)` | 1945 | bind/script | W3 | - | void | url copied to packet; Linux WebView re-calls webui_show | safe in callback; non-blocking (WV: blocks like show) |
| 57 | webui_navigate_client | `void webui_navigate_client(webui_event_t* e, const char* url)` | 1910 | bind/script | W3 | - | void | event struct (or interface_navigate_client) | callback-only |
| 58 | webui_clean | `void webui_clean(void)` | 1991 | wait/close | W1 | - | void | call only after wait returns, from non-callback thread; invalidates ALL WebUI-owned pointers | inside callback = webui_exit() and NO cleanup (webui.c:2000-2007); outside: exit + <=2 s wait + mg_exit_library + free_all_mem + destroy mutexes |
| 59 | webui_delete_all_profiles | `void webui_delete_all_profiles(void)` | 2013 | window | W4 | - | void | fs | BLOCKS on fs |
| 60 | webui_delete_profile | `void webui_delete_profile(size_t window)` | 2207 | window | W4 | - | void | deletes profile folder; fs | BLOCKS on fs |
| 61 | webui_get_parent_process_id | `size_t webui_get_parent_process_id(size_t window)` | 2975 | other | W5 | - | scalar | size_t pid | safe |
| 62 | webui_get_child_process_id | `size_t webui_get_child_process_id(size_t window)` | 3308 | other | W5 | - | scalar | size_t pid (browser child; 0 if none) | safe |
| 63 | webui_win32_get_hwnd | `void* webui_win32_get_hwnd(size_t window)` | 3325 | window | W4 | - | opaque OS handle (not heap, never free) | `hb_retptr`, NIL when NULL; non-Windows always NULL (webui.c:3339-3368); do not convert to number | safe; Windows non-WebView mode scans top-level windows (up to 10000 iterations) |
| 64 | webui_get_hwnd | `void* webui_get_hwnd(size_t window)` | 3371 | window | W4 | - | opaque OS handle (not heap, never free) | `hb_retptr`, NIL when NULL; Windows = HWND, Linux = GtkWindow* only in WebView mode else NULL, macOS NULL (webui.c:3377-3400) | safe |
| 65 | webui_get_port | `size_t webui_get_port(size_t window)` | 3020 | network/port | W5 | - | scalar | size_t port <= 65535 | safe |
| 66 | webui_set_port | `bool webui_set_port(size_t window, size_t port)` | 3281 | network/port | W5 | - | scalar | size_t port; false if in use | safe; reloads running server |
| 67 | webui_get_free_port | `size_t webui_get_free_port(void)` | 3008 | network/port | W5 | - | scalar | probes sockets | safe |
| 68 | webui_set_logger | `void webui_set_logger(void (*func)(size_t level, const char* log, void* user_data), void *user_data)` | 987 | OUT | OUT | - | - | - | excluded by CEO list |
| 69 | webui_set_config | `void webui_set_config(webui_config option, bool status)` | 3187 | config | W1 | - | void | enum passed as int; use GLOBAL setter (see event_blocking note) | safe; set before first show |
| 70 | webui_set_event_blocking | `void webui_set_event_blocking(size_t window, bool status)` | 3265 | config | W4 | - | void | NO-OP in effect: only global config ui_event_blocking is checked (webui.c:13090,13630); bridge must map to webui_set_config(ui_event_blocking) | safe |
| 71 | webui_set_frameless | `void webui_set_frameless(size_t window, bool status)` | 3231 | window | W4 | - | void | bool param passed as int slot | safe |
| 72 | webui_set_transparent | `void webui_set_transparent(size_t window, bool status)` | 3248 | window | W4 | - | void | bool param passed as int slot | safe |
| 73 | webui_get_mime_type | `const char* webui_get_mime_type(const char* file)` | 2996 | network/port | W5 | - | WebUI-owned (static civetweb table) | copy with hb_retc; never free | safe |
| 74 | webui_set_tls_certificate | `bool webui_set_tls_certificate(const char* certificate_pem, const char* private_key_pem)` | 3128 | network/port | W5 | - | scalar | copies PEMs; ON NON-TLS LIB (libwebui-2-static.a) => WEBUI_ASSERT => webui_exit + assert(0) = PROCESS ABORT (webui.c:3181, 5377-5385): bridge must NOT call it unless built with -DWEBUI_TLS (use link-time guard or return false) | safe on TLS build |
| 75 | webui_run | `void webui_run(size_t window, const char* script)` | 1028 | bind/script | W3 | - | void | fire and forget; script copied | safe in callback; non-blocking |
| 76 | webui_run_client | `void webui_run_client(webui_event_t* e, const char* script)` | 994 | bind/script | W3 | - | void | needs event struct (or interface_run_client) | safe in callback; non-blocking |
| 77 | webui_script | `bool webui_script(size_t window, const char* script, size_t timeout, char* buffer, size_t buffer_length)` | 1280 | bind/script | W3 | - | bool | caller buffer zeroed first, filled <= len-1, WebUI keeps nothing after return; **false on JS exception, but the buffer then holds the error text (measured, W0 F2); false on timeout / not connected / multi_client leaves it empty, so false never means "buffer is a result"**; Harbour: preallocate with hb_xgrab, hb_retclen after; timeout 0 or >86400 = WAIT FOREVER | BLOCKS (1 ms poll) until result/timeout; inside callback no deadlock (CMD_JS bypasses mutex_receive) but blocks callback thread and, in ui_event_blocking mode, queues other UI events behind it; multi_client mode returns false |
| 78 | webui_script_client | `bool webui_script_client(webui_event_t* e, const char* script, size_t timeout, char* buffer, size_t buffer_length)` | 1125 | bind/script | W3 | - | bool | as webui_script, event struct needed (or interface_script_client) | BLOCKS like script |
| 79 | webui_set_runtime | `void webui_set_runtime(size_t window, size_t runtime)` | 4587 | bind/script | W3 | - | void | size_t runtime 0..3 | safe |
| 80 | webui_get_count | `size_t webui_get_count(webui_event_t* e)` | 2671 | strings/returns | W2 | - | scalar | hb_retnd / hb_retns; value < 2^31 in practice | safe in callback; returns 0/""/false if event dead |
| 81 | webui_get_int_at | `long long int webui_get_int_at(webui_event_t* e, size_t index)` | 2694 | strings/returns | W2 | - | scalar | int64 return: C99 shim TU, hand to Harbour via hb_retnd(double) (JS numbers are doubles; >2^53 loses precision) | safe in callback; returns 0/""/false if event dead |
| 82 | webui_get_int | `long long int webui_get_int(webui_event_t* e)` | 2802 | strings/returns | W2 | - | scalar | int64 return: C99 shim TU, hand to Harbour via hb_retnd(double) (JS numbers are doubles; >2^53 loses precision) | safe in callback; returns 0/""/false if event dead |
| 83 | webui_get_float_at | `double webui_get_float_at(webui_event_t* e, size_t index)` | 2720 | strings/returns | W2 | - | scalar | hb_retnd | safe in callback; returns 0/""/false if event dead |
| 84 | webui_get_float | `double webui_get_float(webui_event_t* e)` | 2811 | strings/returns | W2 | - | scalar | hb_retnd | safe in callback; returns 0/""/false if event dead |
| 85 | webui_get_string_at | `const char* webui_get_string_at(webui_event_t* e, size_t index)` | 2639 | strings/returns | W2 | - | WebUI-owned | index 0..16 (WEBUI_MAX_ARG=16, 17 slots); NULL if event dead; "" if empty; copy with hb_retc | safe in callback; copy before callback returns |
| 86 | webui_get_string | `const char* webui_get_string(webui_event_t* e)` | 2793 | strings/returns | W1 | - | WebUI-owned | event-owned or literal ""; NULL if event dead; copy with hb_retc (inside callback only) | safe in callback; pointer invalid after callback returns (sync mode) / after return_* (async mode) |
| 87 | webui_get_bool_at | `bool webui_get_bool_at(webui_event_t* e, size_t index)` | 2745 | strings/returns | W2 | - | scalar | hb_retl (bool = _Bool, read as unsigned char; use only low byte) | safe in callback; returns 0/""/false if event dead |
| 88 | webui_get_bool | `bool webui_get_bool(webui_event_t* e)` | 2820 | strings/returns | W2 | - | scalar | hb_retl (bool = _Bool, read as unsigned char; use only low byte) | safe in callback; returns 0/""/false if event dead |
| 89 | webui_get_size_at | `size_t webui_get_size_at(webui_event_t* e, size_t index)` | 2767 | strings/returns | W2 | - | scalar | hb_retnd / hb_retns; value < 2^31 in practice | safe in callback; returns 0/""/false if event dead |
| 90 | webui_get_size | `size_t webui_get_size(webui_event_t* e)` | 2829 | strings/returns | W2 | - | scalar | hb_retnd / hb_retns; value < 2^31 in practice | safe in callback; returns 0/""/false if event dead |
| 91 | webui_return_int | `void webui_return_int(webui_event_t* e, long long int n)` | 2838 | strings/returns | W2 | - | void | long long param -> C99 shim; double->long long cast; response "%lld" (20 B) | safe in callback |
| 92 | webui_return_float | `void webui_return_float(webui_event_t* e, double f)` | 2862 | strings/returns | W2 | - | void | double param; response "%lf" (locale dependent decimal sep) | safe in callback |
| 93 | webui_return_string | `void webui_return_string(webui_event_t* e, const char* s)` | 2888 | strings/returns | W1 | - | void | WebUI copies s (webui.c:2888-2910); caller may free source immediately | safe in callback; last call wins (previous response freed, webui.c:5994-6001) |
| 94 | webui_return_bool | `void webui_return_bool(webui_event_t* e, bool b)` | 2934 | strings/returns | W2 | - | void | bool param passed as int slot; response "%d" | safe in callback |
| 95 | webui_return_http | `void webui_return_http(size_t window, const void* response, int length)` | 2958 | OUT | OUT | - | - | - | excluded by CEO list |
| 96 | webui_get_last_error_number | `size_t webui_get_last_error_number()` | 2914 | other | W2 | - | scalar | stub: always 0 (webui.c:2914) | safe |
| 97 | webui_get_last_error_message | `const char* webui_get_last_error_message()` | 2924 | other | W2 | - | WebUI-owned (literal "") | stub: always "" (webui.c:2924); do not free | safe |
| 98 | webui_interface_bind | `size_t webui_interface_bind(size_t window, const char* element, void (*func)(size_t, size_t, char*, size_t, size_t))` | 4903 | bind/script | W2 | FP: func ptr kept (element copied) | scalar | callback (size_t window,size_t type,char* element,size_t event_number,size_t bind_id): ABI-trivial; element valid only during callback | safe |
| 99 | webui_interface_set_response | `void webui_interface_set_response(size_t window, size_t event_number, const char* response)` | 4925 | strings/returns | W2 | - | void | string copied (webui.c:4925) | safe in callback |
| 100 | webui_interface_is_app_running | `bool webui_interface_is_app_running(void)` | 5012 | wait/close | W2 | - | bool | loop condition for Harbour main loop | safe; sticky false once wait not needed |
| 101 | webui_interface_get_window_id | `size_t webui_interface_get_window_id(size_t window)` | 5042 | window | W2 | - | scalar | identity check | safe |
| 102 | webui_interface_get_string_at | `const char* webui_interface_get_string_at(size_t window, size_t event_number, size_t index)` | 4808 | strings/returns | W2 | - | WebUI-owned | same rules as webui_get_string_at (builds partial event on stack: only window+event_number used) | safe in callback |
| 103 | webui_interface_get_int_at | `long long int webui_interface_get_int_at(size_t window, size_t event_number, size_t index)` | 4827 | strings/returns | W2 | - | scalar | same rules as webui_get_int_at (builds partial event on stack: only window+event_number used) | safe in callback |
| 104 | webui_interface_get_float_at | `double webui_interface_get_float_at(size_t window, size_t event_number, size_t index)` | 4846 | strings/returns | W2 | - | scalar | same rules as webui_get_float_at (builds partial event on stack: only window+event_number used) | safe in callback |
| 105 | webui_interface_get_bool_at | `bool webui_interface_get_bool_at(size_t window, size_t event_number, size_t index)` | 4865 | strings/returns | W2 | - | scalar | same rules as webui_get_bool_at (builds partial event on stack: only window+event_number used) | safe in callback |
| 106 | webui_interface_get_size_at | `size_t webui_interface_get_size_at(size_t window, size_t event_number, size_t index)` | 4884 | strings/returns | W2 | - | scalar | same rules as webui_get_size_at (builds partial event on stack: only window+event_number used) | safe in callback |
| 107 | webui_interface_show_client | `bool webui_interface_show_client(size_t window, size_t event_number, const char* content)` | 5059 | show | W3 | - | bool | ABI-trivial (window,event_number,...); preferred over struct variants | callback-only; script_client/show_client BLOCK |
| 108 | webui_interface_close_client | `void webui_interface_close_client(size_t window, size_t event_number)` | 5086 | wait/close | W3 | - | void | ABI-trivial (window,event_number,...); preferred over struct variants | callback-only; script_client/show_client BLOCK |
| 109 | webui_interface_send_raw_client | `void webui_interface_send_raw_client(size_t window, size_t event_number, const char* function, const void* raw, size_t size)` | 5113 | bind/script | W3 | - | void | ABI-trivial (window,event_number,...); preferred over struct variants | callback-only; script_client/show_client BLOCK |
| 110 | webui_interface_navigate_client | `void webui_interface_navigate_client(size_t window, size_t event_number, const char* url)` | 5140 | bind/script | W3 | - | void | ABI-trivial (window,event_number,...); preferred over struct variants | callback-only; script_client/show_client BLOCK |
| 111 | webui_interface_run_client | `void webui_interface_run_client(size_t window, size_t event_number, const char* script)` | 5167 | bind/script | W3 | - | void | ABI-trivial (window,event_number,...); preferred over struct variants | callback-only; script_client/show_client BLOCK |
| 112 | webui_interface_script_client | `bool webui_interface_script_client(size_t window, size_t event_number, const char* script, size_t timeout, char* buffer, size_t buffer_length)` | 5194 | bind/script | W3 | - | bool | ABI-trivial (window,event_number,...); preferred over struct variants | callback-only; script_client/show_client BLOCK |

## 2. Pointer-retaining functions and reclamation rule

Only function pointers are retained by WebUI (all string/buffer arguments are copied or used only during the call):

- `webui_bind` (line 2552): FP: func ptr kept until window destroy/clean (element string copied)
- `webui_interface_bind` (line 4903): FP: func ptr kept (element copied)
- (out of scope, same class: set_logger, set_close_handler_wv, set_file_handler*, set_context)

Rules:

1. Callback function pointers must be **static C trampolines** (one per bind kind), never Harbour symbols or closures. They stay valid for the process lifetime; WebUI drops them at `webui_destroy` / `webui_clean`.
2. Per-bind Harbour handler lives in a bridge table indexed by `bind_id` (<= 65535); entries are freed (hb_itemRelease) after `webui_destroy(window)` / after `webui_clean`, never from inside the callback of that bind.
3. `element` string passed to bind is copied by WebUI: bridge may free its copy right after the call.
4. Buffers: `webui_script*` buffer is zeroed first and released before return (late result cannot write, webui.c:1248-1249); `webui_send_raw*` copies raw; `webui_return_*` copy (return_string) or build own buffer. Caller never has to keep anything alive.
5. Returned `WebUI-owned` pointers die at: callback return (event strings), next `webui_get_url` of same window, `webui_destroy(window)`, `webui_clean`. Always copy with `hb_retc` immediately; never store.
6. `caller-owned` results (`webui_encode`, `webui_decode`): `hb_retc` copy then `webui_free(ptr)`. `webui_free` is a **no-op after webui_exit** (webui.c:4155-4157): after exit the buffers are reclaimed only by `webui_clean` -> bridge should free encode/decode results before exit or accept the (bounded) leak until clean.
7. After `webui_clean()` every tracked pointer is freed (`_webui_free_all_mem`) and `initialized=false`; any cached WebUI pointer in the bridge is dangling. Restart-after-clean (new_window after clean) must be verified in W1 as a test, not assumed.

## 3. Thread-safety facts for Harbour

- Model: civetweb thread per WebSocket packet; user callbacks run on WebUI threads (not the Harbour main thread). Harbour VM is not re-entrant without MT/hb_vmThreadInit. Recommended bridge pattern: `webui_set_config(asynchronous_response,true)` + trampoline queues (window,event_number,bind_id) to a mutex-protected queue; Harbour main loop (webui_wait_async / interface_is_app_running) dequeues, runs the handler, calls return_*; in async mode event data stays valid until return_* (webui.c:6004-6033, 5993-5998).
- Blocking (never from UI-sensitive thread): wait (until all windows closed or exit; timeout 0 = until exit), show/show_browser/show_wv/show_client/start_server/get_url (<= startup_timeout, default 15 s, max 60 s, while show_wait_connection), script/script_client (1 ms poll; timeout 0 or >86400 = forever), exit (<= ~2 s), clean (exit + <= 2 s + free), destroy (<= ~4 s outside callback), is_high_contrast (popen), delete_profile(s), set_icon_file (file I/O).
- Safe from a callback: get_*/interface_get_*, return_*/interface_set_response, run/run_client, send_raw*, navigate*, close/close_client, exit (blocks 2 s), set_* window props, bind. Event data pointers valid only while the callback runs (sync) or until return_* (async).
- `webui_script` inside a callback: **does not deadlock** in either mode: result packets `WEBUI_CMD_JS` bypass `mutex_receive` (webui.c:13090,13630 region). It blocks the callback thread until result/timeout, and in `ui_event_blocking=true` the held `mutex_receive` makes all other UI events queue behind it. With timeout 0 it can hang forever if the client never answers. Recommendation: never call script from a callback with timeout 0; use `webui_run` for no-result calls.
- `webui_clean` inside a callback: does **not** clean. It calls `webui_exit()` (blocks ~2 s) and returns (webui.c:2000-2007; `_webui_callback_win` is thread-local). Memory is reclaimed only by a second `webui_clean()` after `webui_wait()` returns, on a non-callback thread. Bridge: if trampoline defers handlers to the main thread, the main thread is not "in callback" and webui_clean performs full cleanup.
- `webui_destroy` inside the callback of that window defers (flag + return, webui.c:1675-1701); outside it waits for the window thread.
- `webui_set_event_blocking(window,..)` is effectively a no-op; use global `webui_set_config(ui_event_blocking, ..)` (webui.c:13090,13630 only check the global).
- WebView mode (Linux GTK): wait/show/script pump GTK on the **calling thread** -> all show/wait/script calls must be on the Harbour main thread.
- `webui_set_tls_certificate` on libwebui-2-static.a (non-TLS): aborts the process (WEBUI_ASSERT -> panic -> webui_exit + assert). Must be guarded or stubbed in the bridge (return false).
- `webui_start_server` has a global side effect `webui_set_timeout(0)` (webui.c:2277).

## 4. Type facts (feeds T0.2)

- bool returns (20): show, show_client, show_browser, show_wv, is_high_contrast, browser_exist, wait_async, set_root_folder, set_default_root_folder, is_shown, set_port, set_tls_certificate, script, script_client, get_bool_at, get_bool, interface_is_app_running, interface_get_bool_at, interface_show_client, interface_script_client
- bool params: set_kiosk, set_high_contrast, set_resizable, set_hide, set_public, set_frameless, set_transparent, set_event_blocking, set_config(status), return_bool.
- enum-typed param: webui_set_config (webui_config -> int). All other enum-ish values are size_t.
- int64 (`long long int`): webui_get_int_at, webui_get_int, webui_return_int, webui_interface_get_int_at
- size_t ids: window <= 65535 (WEBUI_MAX_IDS=UINT16_MAX); bind_id <= 65535; event_number = 15-bit generation + 16-bit slot < 2^31; client/connection ids < 65535; port <= 65535; pid < 2^32; get_size_at <= 64,000,000 (WEBUI_MAX_BUF). All exact in a double (<2^53) on Win32 and Linux64. The only unbounded size_t inputs are timeouts/sizes: validate in the bridge (negative double -> huge size_t is UB).
- Hard limits to validate in the bridge: set_size 100..3840 x 100..2160; set_position 0..3000 x 0..1800 (else silently ignored); timeout <= 60 (set_timeout) and <= 86400 (script); arg index <= 16.


## 5. Extra observations (verified by code or by a /tmp probe, libwebui-2-static.a from /tmp/webui-build/dist)

- `webui_interface_is_app_running()` called before any window server has started returned 0 and latches false (static `app_is_running`, webui.c:5019-5039). Harbour main loop must call it only after the first show/start_server, or use `webui_wait_async()` instead.
- Hand-declared prototypes (bool return as `unsigned char`, bool param as `int`, size_t) linked to libwebui-2-static.a with gcc -std=gnu89 work: `set_root_folder("/tmp")=1`, `("/nonexistent")=0`, `is_shown=0`, `encode("hello")="aGVsbG8="`, `set_public(w,1)` -> listening socket 0.0.0.0 in /proc/net/tcp (public) - observable headless, no browser needed.
- `webui_start_server` returns `http://localhost:<port>`; port also available through `webui_get_port` once the server thread started.
- Chrome (/usr/local/bin/google-chrome), Xvfb and objdump are present on this VM; `ss`, hbmk2 and a 32-bit multilib are not.

## 6. T0.2 ABI test design (design only, nothing implemented)

Goal: prove the bridge's hand-written ABI assumptions against the real library, not against the header, on Linux x86-64 (gate). Win32 is a re-run of the same sources (deferred).

Proposed layout (to be created in W0 implementation, e.g. poc/harbour-webui/tests/abi/): `abi_bridge.c` (C89, `-std=c89 -pedantic-errors`, hand declarations exactly as the bridge will have them: `typedef unsigned char ubool;` bool params `int`, enum `int`, ids `size_t`, mirrored `webui_event_t`), `abi_real.c` (C99, includes real `include/webui.h`, reference side), `abi_i64.c` (C99 shim: only place that may say `long long`), `run-abi.sh`.

| ID | Assumption | Method | Pass criterion |
|---|---|---|---|
| A1 | bool is 1 byte, enum webui_config is int, size_t = pointer size, long long = 8 | C11 `_Static_assert` in abi_real.c (real header) | all hold |
| A2 | Mirrored `webui_event_t` equals real | export sizeof+offsetof of 8 fields from both TUs, compare at runtime | identical (64 B on x86-64, 32 B on x32) |
| A3 | Enum numbers (webui_config, webui_browser 0..13, webui_runtime 0..3, webui_event 0..4, logger level) | script greps header, generates `abi_enums.h`; bridge constants compared | 100 percent identical; script fails if header gains/changes names (drift check) |
| A4 | Prototype drift | script parses all 112 `WEBUI_EXPORT` prototypes, normalizes bool->ubool/int, compares with bridge declarations of the 104 in-scope names | zero diffs, zero missing, zero extra |
| B1 | bool return read as `unsigned char` | objdump -d of each bool-returning function in the .a: epilogue writes only `%al` (`setcc`, `mov $0/1,%al/%eax`); informational | no function relies on upper bytes; list attached to report |
| C1 | bool return runtime | pairs giving both 1 and 0 headless: `set_root_folder("/tmp")`/`("/nonexistent")`, `set_port(used)`/`set_port(free)`, `is_shown`, `browser_exist`, `interface_is_app_running`; call between each an asm/noinline function that fills rax/eax with 0xDEADBEEF (dirty-register control) | 1000 iterations, every value in {0,1}, equal to the value read through the real-header `bool` call, 0 mismatches. Negative control (read as int) is informational only |
| C2 | bool param as int | `webui_set_public(w,1)` then `webui_start_server`: listener is 0.0.0.0; `(w,0)`: 127.0.0.1 (parse /proc/net/tcp). Called through int-declared prototype and through real bool prototype | identical observable result; bridge helper normalizes `hb_parl ? 1 : 0`; passing 256 documented as unsupported |
| C3 | enum param as int | `webui_set_config(int,int)` for each config value; effect observed in E (asynchronous_response, ui_event_blocking, show_wait_connection) | each setting produces its documented observable effect; no crash |
| D1 | size_t through double | bridge helpers `hb_n2size()` / `size2hb()` over {0,1,65535,2^31-1,2^32-1,2^53}; also 2^53+1, 2^64, -1, NaN, 1.5 | exact round trip up to 2^53; others rejected (error, not UB) |
| D2 | real id ranges | 200 windows via new_window/new_window_id (<=65535), event_number from 10 000 live events, port, pid | all values < 2^31 (event_number) / <= 65535 (ids) and round-trip through double |
| D3 | int64 | abi_i64.c: `return_int(LLONG_MIN, LLONG_MAX, 0, -1)` observed by JS as the decimal string; JS sends string "9223372036854775807" and `get_int_at` returns LLONG_MAX | exact decimal equality; `int64_to_hb` exact for \|v\|<=2^53 and signals overflow (returns string) beyond |
| E1 | live callback path (Xvfb + Chrome, same recipe as PoC-001 acceptance) | page calls `webui.call('f', 123, 'x', true, 1.5)`; trampoline using mirrored struct and `webui_bind` | count = number of JS args (4: 123, 'x', true, 1.5; the function name is not counted), get_int_at(0)=123, string "x", bool 1, float 1.5, element "f", window id equal, callback thread != main thread |
| E2 | script result + bool return | `webui_script(w,"return 40+2;",5,buf,64)` then a throwing script | true + "42"; false for error; buffer zeroed first |
| E3 | async response | `set_config(asynchronous_response,1)`, trampoline queues event, main thread calls return_* later | JS receives the late value; event_number stays valid until return |

Run on Linux with the real static library:

```
make                                  # produces dist/libwebui-2-static.a (non-TLS, non-WebView variant)
gcc -std=c89 -pedantic-errors -Wall -Wextra -O2 -c abi_bridge.c
gcc -std=c99 -O2 -Iinclude -c abi_real.c abi_i64.c
gcc abi_bridge.o abi_real.o abi_i64.o dist/libwebui-2-static.a -lpthread -lm -ldl -o abi_test
./abi_test --headless                 # A, B, C, D1, D3(partial): no DISPLAY needed
xvfb-run -a -s "-screen 0 1280x1024x24" ./abi_test --live   # E1-E3, D2, D3
```

Matrix: {-O0, -O2} x {gcc 13, +ASan/UBSan build of the bridge TU}; output TAP (`ok`/`not ok`), exit code != 0 on any failure. Hard blockers for Win32 re-run (deferred): `-m32` multilib (absent here) or vs2019-x32; expected differences are only sizeof(struct)=32 and `long long` passed in two stack slots.

Known gaps in this design: B1 is x86-64 only; bool-in-AL on x86 (BCC/MSVC) is reasoned, not measured; `webui_set_tls_certificate` is excluded from runtime tests because it aborts the process on the non-TLS library.

## 7. W0 exit review (Architect, after Back-end W0 implementation, commits 4ef7ed7..cabf7ba)

Reviewed: poc/harbour-webui/w0/W0-REPORT.md, functions.txt, gen_functions.py, check_protos.py, check_bool_abi.py, abi_*.c, run-abi.sh, results/. Re-ran `gen_functions.py` (18 ok) and `check_protos.py` (14 ok) here; independent count of functions.txt at that time: 112 rows, waves 12/27/27/25/11, excluded 10, area totals 28/15/10/9/24/2/7/3/4 equal to spec section 1.

### 7.1 Function list verdict: APPROVED

[Review state at commit a568c06; superseded by the set_context decision: now 104 in scope, 8 excluded, waves 12/29/27/25/11.] 112 header functions, 102 in scope, 10 excluded; waves 12/27/27/25/11. The excluded set equalled the 10 names of spec section 1; the W1 set equals the 12 names of spec section 7; hwnd functions are W4. No misplaced function found. This audit table (section 1) now matches functions.txt row for row (rows 63 and 64 corrected, summary lines corrected).

### 7.2 Back-end findings and effect on spec 2.1

| Finding | Effect on bridge rules | Required spec text change (for PM, spec not edited by Architect) |
|---|---|---|
| F1: E1 expected 5 args, measured 4 | Audit typo only: `webui.call` first parameter is the function name, not an argument. `get_count` = JS args only, index 0 = first argument. Rule 9 (index <= 16) unchanged. | None to 2.1. Optional note in spec 7 W2 test row: "`get_count` excludes the function name". |
| F2: `webui_script` false on JS exception, buffer holds error text | Changes the wrapper contract. Spec 2.1 and 3 only talk about buffer lifetime, not result semantics. | Add rule 11 to 2.1: "`WEBUI_SCRIPT` returns two values, success flag and buffer text. The buffer is a result only when the flag is true. When the flag is false the text is the JS error message after an exception, and empty after timeout, no connection or multi_client; the wrapper must return it as error text, never as a result. The buffer is zeroed before the call and WebUI keeps no pointer after return." Add to W3 required tests: exception, timeout, truncation to size-1, late result not written. |
| F3: no bool high-byte pollution observed on this binary | No change; B shows 18 exits may leave undefined high bytes. | Keep section 4 line "bool return declared `unsigned char`" and append: "reading as `int` is forbidden even if tests cannot reproduce pollution; bool parameters normalized with `? 1 : 0`." |
| F4: hwnd rows | Audit fixed (this commit). | None. |
| F5: C99 cross-compile in one TU impossible (`ubool` vs `bool`, duplicate struct) | No bridge rule change. | Replace the last bullet of section 4 with: "Declaration conformance is proven by `abi_real.c` compiled against the real header (C99) plus the A4 script (all 112 prototypes, normalized) with zero diffs." |
| F6: O2 headless run about 95 s (3000-call cases) | None. | None; keep 1000 iterations in section 7.1 C group, or allow 300 for `set_port` and `browser_exist` only if CI time requires (PM decision). |
| F7: ASan LeakSanitizer is not a valgrind substitute | See 7.3. | A8 wording change, 7.3. |
| F8: client_id / connection_id always 0 in single client mode | None in W1/W2. | W3 test note: multi-client behaviour is checked only if multi_client tests are added. |
| `folder_monitor` effect not observed | No bridge rule change: `webui_set_config` just forwards the enum as int; effect is the library's. But test coverage is incomplete. | Spec 5 A6 / W1 test: "set_config is tested for all six values; five have an observed effect, `folder_monitor` is call-only (no crash) until W5, where `set_root_folder` plus a file change on a shown folder window observes it". Add that observation to the W5 required tests. |

### 7.3 A8 (valgrind): can be met; fallback plan

valgrind 3.22.0 **is** installed on this agent VM (`/usr/bin/valgrind`), unlike the Back-end environment, so the evidence can be produced here or in CI. Trial on the W0 binary (`valgrind --leak-check=full --errors-for-leak-kinds=definite --error-exitcode=9 -q build/O0/abi_test --headless`, 2 min, all 57 TAP ok): exit 9 because of exactly **2 definite-loss records, both rooted in `dlopen` constructors of GTK/glib** (loaded by `webui_set_center` through `_webui_load_gtk_and_webkit`); **0 records with WebUI or bridge frames**. This is the third-party class of spec risk 7.

Proposed A8 text: "valgrind `--leak-check=full` reports 0 definite leaks with WebUI or bridge frames; leaks whose stack is rooted in `dlopen`/`_dl_init` of system libraries (GTK, glib) are matched by a committed suppression file and listed separately in the report; allocation and release counters read 0 after `WEBUI_CLEAN`."

Fallback if valgrind cannot be installed (Back-end environment): evidence plan, all three required together:
1. ASan with LeakSanitizer (`ASAN_OPTIONS=detect_leaks=1`) over the full wave test run; the library itself is not instrumented, so LSan sees `malloc` interposition and reports unfreed blocks reachable by no pointer at exit (works for the static library too).
2. Bridge allocation counters in a debug build (`hb_xgrab`/`hb_xfree` wrappers and the `webui_encode`/`decode` `webui_free` pairs, plus handler-table entries), asserted 0 after `WEBUI_CLEAN`; negative control: a deliberately leaked block must make the counter non-zero.
3. Repeat-cycle test: new_window, show, bind, call, destroy, clean x 20; RSS growth after warm-up below a fixed threshold, reported as evidence of no unbounded growth.
Note for the report: WebUI-internal tracked memory is only reclaimed by `webui_clean` (section 2 rule 7), so counters and LSan must be read after `webui_clean`, never at `webui_exit`.

## 8. Amendment: set_context and get_context in scope (W2)

CEO decision approved by the user. Counts: excluded 8 (malloc, memcpy, set_logger, set_close_handler_wv, set_file_handler, set_file_handler_window, return_http, interface_set_response_file_handler), in scope 104, waves W1=12, W2=29, W3=27, W4=25, W5=11. Back-end `functions.txt`, `gen_functions.py` and the spec must be updated in parallel to these numbers (area counts: bind/script 15 -> 17).

Bridge rules for the pair (proposal for spec 2.1, not edited here):
1. The context is a bridge-side slot table: the Harbour value is copied into bridge storage (hb_itemNew/hb_itemCopy), the slot number is passed to WebUI as `void*` (slot 0 = none/NULL); no Harbour pointer or item address ever reaches WebUI.
2. Setting a context on an element that already has one releases the previous slot; passing NIL releases it and stores NULL.
3. All slots are released after `webui_clean` (or `webui_destroy` of the window for that window's slots), never inside a callback of that element.
4. `webui_set_context` calls `webui_bind(window, element, NULL)` and returns the bind id internally unused: the element becomes registered with no handler, and writes `cb_context[cb_index]` even if the bind failed (index 0, webui.c:2542-2545). The wrapper must call it only for a valid window and a non-empty element, and tests must cover "set_context on an unbound element then bind", to confirm the later `webui_bind` handler and the context coexist.
5. `webui_get_context` reads by (window, event element): in the queued-event design (2.1 rule 3) the queued record must carry the element name, or the bind id, so the main thread can look the slot up after the callback returned.

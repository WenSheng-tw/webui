/*
 * bridge_decl.h  --  hand-written WebUI declarations for the PoC-002 bridge (ISO C89).
 *
 * Conventions (spec 4): bool results are declared ubool (unsigned char) and read through
 * their low byte only; bool and enum parameters are int; ids, sizes and browser/runtime
 * values are size_t; webui_event_t stays opaque here. The four long long functions are
 * not declared here, they live in the C99 unit (abi_i64.c now, bridge_i64.c in W1).
 * The eight excluded functions are not declared at all.
 *
 * w0/check_protos.py diffs this file and abi_i64.c against include/webui.h.
 */

#ifndef BRIDGE_DECL_H
#define BRIDGE_DECL_H

#include <stddef.h>

#include "bridge_consts.h"

typedef unsigned char ubool;
typedef struct webui_event_t webui_event_t;

extern size_t webui_new_window(void);
extern size_t webui_new_window_id(size_t window_number);
extern size_t webui_get_new_window_id(void);
extern size_t webui_bind(size_t window, const char *element, void (*func)(webui_event_t *e));
extern void webui_set_context(size_t window, const char *element, void *context);
extern void *webui_get_context(webui_event_t *e);
extern size_t webui_get_best_browser(size_t window);
extern ubool webui_show(size_t window, const char *content);
extern ubool webui_show_client(webui_event_t *e, const char *content);
extern ubool webui_show_browser(size_t window, const char *content, size_t browser);
extern const char *webui_start_server(size_t window, const char *content);
extern ubool webui_show_wv(size_t window, const char *content);
extern void webui_set_kiosk(size_t window, int status);
extern void webui_focus(size_t window);
extern void webui_set_custom_parameters(size_t window, char *params);
extern void webui_set_high_contrast(size_t window, int status);
extern void webui_set_resizable(size_t window, int status);
extern ubool webui_is_high_contrast(void);
extern ubool webui_browser_exist(size_t browser);
extern void webui_wait(void);
extern ubool webui_wait_async(void);
extern void webui_close(size_t window);
extern void webui_minimize(size_t window);
extern void webui_maximize(size_t window);
extern void webui_close_client(webui_event_t *e);
extern void webui_destroy(size_t window);
extern void webui_exit(void);
extern ubool webui_set_root_folder(size_t window, const char *path);
extern void webui_set_browser_folder(const char *path);
extern ubool webui_set_default_root_folder(const char *path);
extern ubool webui_is_shown(size_t window);
extern void webui_set_timeout(size_t second);
extern void webui_set_icon(size_t window, const char *icon, const char *icon_type);
extern void webui_set_icon_file(size_t window, const char *path);
extern char *webui_encode(const char *str);
extern char *webui_decode(const char *str);
extern void webui_free(void *ptr);
extern void webui_send_raw(size_t window, const char *function, const void *raw, size_t size);
extern void webui_send_raw_client(webui_event_t *e, const char *function, const void *raw, size_t size);
extern void webui_set_hide(size_t window, int status);
extern void webui_set_size(size_t window, unsigned int width, unsigned int height);
extern void webui_set_minimum_size(size_t window, unsigned int width, unsigned int height);
extern void webui_set_position(size_t window, unsigned int x, unsigned int y);
extern void webui_set_center(size_t window);
extern void webui_set_profile(size_t window, const char *name, const char *path);
extern void webui_set_proxy(size_t window, const char *proxy_server);
extern const char *webui_get_url(size_t window);
extern void webui_open_url(const char *url);
extern void webui_set_public(size_t window, int status);
extern void webui_navigate(size_t window, const char *url);
extern void webui_navigate_client(webui_event_t *e, const char *url);
extern void webui_clean(void);
extern void webui_delete_all_profiles(void);
extern void webui_delete_profile(size_t window);
extern size_t webui_get_parent_process_id(size_t window);
extern size_t webui_get_child_process_id(size_t window);
extern void *webui_win32_get_hwnd(size_t window);
extern void *webui_get_hwnd(size_t window);
extern size_t webui_get_port(size_t window);
extern ubool webui_set_port(size_t window, size_t port);
extern size_t webui_get_free_port(void);
extern void webui_set_config(int option, int status);
extern void webui_set_event_blocking(size_t window, int status);
extern void webui_set_frameless(size_t window, int status);
extern void webui_set_transparent(size_t window, int status);
extern const char *webui_get_mime_type(const char *file);
extern ubool webui_set_tls_certificate(const char *certificate_pem, const char *private_key_pem);
extern void webui_run(size_t window, const char *script);
extern void webui_run_client(webui_event_t *e, const char *script);
extern ubool webui_script(size_t window, const char *script, size_t timeout, char *buffer, size_t buffer_length);
extern ubool webui_script_client(webui_event_t *e, const char *script, size_t timeout, char *buffer, size_t buffer_length);
extern void webui_set_runtime(size_t window, size_t runtime);
extern size_t webui_get_count(webui_event_t *e);
extern double webui_get_float_at(webui_event_t *e, size_t index);
extern double webui_get_float(webui_event_t *e);
extern const char *webui_get_string_at(webui_event_t *e, size_t index);
extern const char *webui_get_string(webui_event_t *e);
extern ubool webui_get_bool_at(webui_event_t *e, size_t index);
extern ubool webui_get_bool(webui_event_t *e);
extern size_t webui_get_size_at(webui_event_t *e, size_t index);
extern size_t webui_get_size(webui_event_t *e);
extern void webui_return_float(webui_event_t *e, double f);
extern void webui_return_string(webui_event_t *e, const char *s);
extern void webui_return_bool(webui_event_t *e, int b);
extern size_t webui_get_last_error_number(void);
extern const char *webui_get_last_error_message(void);
extern size_t webui_interface_bind(size_t window, const char *element, void (*func)(size_t, size_t, char *, size_t, size_t));
extern void webui_interface_set_response(size_t window, size_t event_number, const char *response);
extern ubool webui_interface_is_app_running(void);
extern size_t webui_interface_get_window_id(size_t window);
extern const char *webui_interface_get_string_at(size_t window, size_t event_number, size_t index);
extern double webui_interface_get_float_at(size_t window, size_t event_number, size_t index);
extern ubool webui_interface_get_bool_at(size_t window, size_t event_number, size_t index);
extern size_t webui_interface_get_size_at(size_t window, size_t event_number, size_t index);
extern ubool webui_interface_show_client(size_t window, size_t event_number, const char *content);
extern void webui_interface_close_client(size_t window, size_t event_number);
extern void webui_interface_send_raw_client(size_t window, size_t event_number, const char *function, const void *raw, size_t size);
extern void webui_interface_navigate_client(size_t window, size_t event_number, const char *url);
extern void webui_interface_run_client(size_t window, size_t event_number, const char *script);
extern ubool webui_interface_script_client(size_t window, size_t event_number, const char *script, size_t timeout, char *buffer, size_t buffer_length);

#endif

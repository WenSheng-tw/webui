/*
 * bridge_int.h  --  bridge internals shared by the bridge_*.c units (ISO C89).
 *
 * Layout of the bridge (spec section 2):
 *   bridge_core.c   lifecycle, bind-id handler table, argument and range helpers,
 *                   allocation counters, WEBUI_BIND / WAIT / WAIT_ASYNC / EXIT / CLEAN /
 *                   SET_TIMEOUT / SET_CONFIG and the test statistics
 *   bridge_win.c    WEBUI_NEW_WINDOW / SHOW / CLOSE
 *   bridge_event.c  the bind trampolines (plain and interface), the main-thread event queue
 *                   and its dispatcher, WEBUI_GET_STRING / RETURN_STRING / the interface
 *                   response, WEBUI_PUMP
 *   bridge_misc.c   W2: the event argument getters and return_*, encode / decode / free,
 *                   last_error, interface_* getters, window id, app running (needs
 *                   bridge_i64.c)
 *   bridge_i64.c    the only C99 unit: the four long long WebUI calls
 *   bridge.c        PoC-001 compatibility names (WEBUI_NEWWINDOW, WEBUI_BINDHB)
 *
 * Threading: WebUI calls the trampoline on its own threads. The trampoline only touches
 * the mutex-protected queue and WebUI's own thread-safe response API. Every Harbour VM
 * call happens on the main thread, from the dispatcher (rule 3).
 */

#ifndef BRIDGE_INT_H
#define BRIDGE_INT_H

#include <stddef.h>

#include "bridge_decl.h"
#include "bridge_hb.h"

/* Mirror of struct webui_event_t (include/webui.h 2.5.0-beta.4), verified by w0 group A2. */
struct webui_event_t
{
    size_t window;
    size_t event_type;
    char *element;
    size_t event_number;
    size_t bind_id;
    size_t client_id;
    size_t connection_id;
    char *cookies;
};

/* Spec 2.1 rule 9 limits. */
#define BRIDGE_ID_MAX 65535UL
#define BRIDGE_TIMEOUT_MAX 60UL
#define BRIDGE_SCRIPT_TIMEOUT_MAX 86400UL
#define BRIDGE_WIN_W_MIN 100UL
#define BRIDGE_WIN_W_MAX 3840UL
#define BRIDGE_WIN_H_MIN 100UL
#define BRIDGE_WIN_H_MAX 2160UL
#define BRIDGE_POS_X_MAX 3000UL
#define BRIDGE_POS_Y_MAX 1800UL
#define BRIDGE_ARG_INDEX_MAX 16UL

#define BRIDGE_HANDLER_NAME_MAX 64

/* Queue node for one bound-function call (bridge_event.c, allocated by the trampoline).
 * It keeps numbers and a copy of the element name only, never WebUI's event pointer: WebUI
 * functions are called with a temporary event built from window and event number, which
 * WebUI validates (a finished or unknown event is ignored), so nothing can dangle. */
struct bridge_event
{
    struct bridge_event *next;
    char *element;
    size_t window;
    size_t event_type;
    size_t event_number;
    size_t bind_id;
};

/* ---- bridge_core.c: counted allocation ---------------------------------------------- */
void *bridge_malloc(size_t n);
void bridge_free(void *p);
char *bridge_strdup(const char *s);

/* ---- bridge_core.c: lifecycle ------------------------------------------------------- */
void bridge_lock(void);
void bridge_unlock(void);
void bridge_mark_dirty(void);
int bridge_ensure_async(void);

/* ---- bridge_core.c: handler table keyed by bind id ---------------------------------- */
int bridge_table_set(size_t bind_id, size_t window, const char *name);
/* The returned name is owned by the table; copy it before any call that can clear it. */
const char *bridge_table_get(size_t bind_id);
size_t bridge_table_count(void);
void bridge_table_clear(void);

/* ---- bridge_core.c: Harbour argument helpers ---------------------------------------- */
/* Each helper returns 1 on success. On failure it raises a Harbour argument error (EG_ARG)
 * and returns 0, and the caller must return to the VM immediately. */
int bridge_raise(const char *what);
int bridge_arg_size(int iParam, size_t lo, size_t hi, const char *what, size_t *out);
int bridge_arg_window(int iParam, size_t *out);
int bridge_arg_bool(int iParam, int *out);
int bridge_arg_string(int iParam, const char *what, const char **out);

/* Pure range predicates of rule 9, used by the helpers and by the unit test aid. */
int bridge_range_ok(size_t v, size_t lo, size_t hi);
int bridge_range_window_size(size_t w, size_t h);
int bridge_range_window_pos(size_t x, size_t y);
int bridge_range_script_timeout(size_t seconds);

/* ---- bridge_event.c ----------------------------------------------------------------- */
void bridge_on_event(webui_event_t *e);
void bridge_on_iface_event(size_t window, size_t event_type, char *element, size_t event_number, size_t bind_id);
int bridge_pump(void);
int bridge_in_dispatch(void);
size_t bridge_queue_length(void);
void bridge_queue_discard(void);
void bridge_answer_all(void);
/* Statistics for tests: 0 queued, 1 dispatched, 2 auto-responded, 3 handler-missing. */
size_t bridge_event_stat(int which);
/* The event being dispatched. Fills *tmp (window, event number, element, bind id, type) and
 * returns 1; outside a handler it raises "<fn> is valid only inside a bound handler" and
 * returns 0. */
int bridge_cur_event(const char *fn, webui_event_t *tmp);
/* 1 if the current event had no answer yet (and marks it answered), 0 if it was answered. */
int bridge_claim_answer(void);
/* Like bridge_claim_answer for an explicit event: 0 only if it is the current event and was
 * answered already; any other event number is passed through (WebUI ignores stale ones). */
int bridge_claim_event(size_t window, size_t event_number);

/* ---- bridge_core.c: shared by bridge.c and HB_FUN_WEBUI_BIND ------------------------ */
/* Returns the bind id, 0 on any failure (nothing raised). */
size_t bridge_bind(size_t window, const char *element, const char *handler);
int bridge_new_window(size_t *window);
void bridge_mark_exited(void);
int bridge_is_exited(void);
void bridge_mark_shown(void);
int bridge_is_shown(void);
/* Counters of the W2 memory paths (WebUI results freed by the bridge, context slots). */
void bridge_note_webui_result(void);
void bridge_note_webui_free(void);

/* ---- bridge_core.c: context slot table (rule 11) ------------------------------------ */
/* Returns a copy of the stored value as a new Harbour item (release with hb_itemRelease),
 * or NULL for slot 0 or an unknown slot. */
void *bridge_context_value(size_t slot);

/* ---- bridge_i64.c (C99): the long long WebUI calls ---------------------------------- */
/* Return 0 and *num when the value is exact in a double (|v| <= 2^53), 1 and the decimal text
 * in str otherwise. e is a webui_event_t pointer. */
int bridge_get_int_at(void *e, size_t index, double *num, char *str, size_t strsize);
int bridge_get_int(void *e, double *num, char *str, size_t strsize);
int bridge_interface_get_int_at(size_t window, size_t event_number, size_t index, double *num, char *str,
                                size_t strsize);
/* 1 if sent to webui_return_int, 0 if the input was not an exact integer (nothing sent). */
int bridge_return_int(void *e, double d);
/* Same for decimal text: optional '-', 1 to 19 digits, within long long. */
int bridge_return_int_text(void *e, const char *s);
/* hb_retnd of an id or count; values above 2^53 cannot occur for ids and are returned as 0. */
void bridge_ret_size(size_t v);

#endif

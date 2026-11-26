/*
 * bridge_int.h  --  bridge internals shared by the bridge_*.c units (ISO C89).
 *
 * Layout of the bridge (spec section 2):
 *   bridge_core.c   lifecycle, bind-id handler table, argument and range helpers,
 *                   allocation counters, WEBUI_BIND / WAIT / WAIT_ASYNC / EXIT / CLEAN /
 *                   SET_TIMEOUT / SET_CONFIG and the test statistics
 *   bridge_win.c    WEBUI_NEW_WINDOW / SHOW / CLOSE
 *   bridge_event.c  the bind trampoline, the main-thread event queue and its dispatcher,
 *                   WEBUI_GET_STRING / RETURN_STRING, WEBUI_PUMP
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

/* Queue node for one bound-function call (bridge_event.c, allocated by the trampoline). */
struct bridge_event
{
    struct bridge_event *next;
    webui_event_t *e;
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
int bridge_pump(void);
int bridge_in_dispatch(void);
size_t bridge_queue_length(void);
void bridge_queue_discard(void);
void bridge_answer_all(void);
/* Statistics for tests: 0 queued, 1 dispatched, 2 auto-responded, 3 handler-missing. */
size_t bridge_event_stat(int which);

/* ---- bridge_core.c: shared by bridge.c and HB_FUN_WEBUI_BIND ------------------------ */
/* Returns the bind id, 0 on any failure (nothing raised). */
size_t bridge_bind(size_t window, const char *element, const char *handler);
size_t bridge_new_window(void);
/* hb_retnd of an id or count; values above 2^53 cannot occur for ids and are returned as 0. */
void bridge_ret_size(size_t v);

#endif

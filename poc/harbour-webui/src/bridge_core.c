/*
 * bridge_core.c  --  bridge lifecycle, handler table, argument checks (ISO C89).
 *
 * See bridge_int.h for the file layout. Everything here runs on the main thread except
 * bridge_malloc / bridge_free / bridge_lock / bridge_unlock, which the WebUI-thread
 * trampoline also uses.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

#include "bridge_int.h"
#include "bridge_num.h"

#define HB_ERR_FUNCNAME_PTR ((const char *) 1)
#define HB_ERR_ARGS_BASE 0xFFFFFFFFUL
#define HB_EG_ARG 1U
#define HB_ERR_SUB_ARGS 3012U

/* ---- mutex and counted allocation --------------------------------------------------- */

#ifdef _WIN32
static CRITICAL_SECTION s_mutex;
static volatile LONG s_mutex_state = 0;

void bridge_lock(void)
{
    if (InterlockedCompareExchange(&s_mutex_state, 1, 0) == 0)
    {
        InitializeCriticalSection(&s_mutex);
        InterlockedExchange(&s_mutex_state, 2);
    }
    while (s_mutex_state != 2)
    {
        Sleep(0);
    }
    EnterCriticalSection(&s_mutex);
}

void bridge_unlock(void)
{
    LeaveCriticalSection(&s_mutex);
}
#else
static pthread_mutex_t s_mutex = PTHREAD_MUTEX_INITIALIZER;

void bridge_lock(void)
{
    pthread_mutex_lock(&s_mutex);
}

void bridge_unlock(void)
{
    pthread_mutex_unlock(&s_mutex);
}
#endif

union bridge_hdr
{
    size_t n;
    double d;
    void *p;
};

static size_t s_allocs = 0;
static size_t s_frees = 0;
static size_t s_live_bytes = 0;

void *bridge_malloc(size_t n)
{
    union bridge_hdr *h;

    h = (union bridge_hdr *) malloc(sizeof(union bridge_hdr) + n);
    if (h == NULL)
    {
        return NULL;
    }
    h->n = n;
    bridge_lock();
    s_allocs++;
    s_live_bytes += n;
    bridge_unlock();
    return (void *) (h + 1);
}

void bridge_free(void *p)
{
    union bridge_hdr *h;

    if (p == NULL)
    {
        return;
    }
    h = ((union bridge_hdr *) p) - 1;
    bridge_lock();
    s_frees++;
    s_live_bytes -= h->n;
    bridge_unlock();
    free(h);
}

char *bridge_strdup(const char *s)
{
    size_t n;
    char *d;

    n = strlen(s);
    d = (char *) bridge_malloc(n + 1);
    if (d != NULL)
    {
        memcpy(d, s, n + 1);
    }
    return d;
}

/* ---- lifecycle state ---------------------------------------------------------------- */

static int s_async = 0;
static int s_dirty = 0;
static int s_clean_pending = 0;
static int s_exited = 0;
static int s_shown = 0;
static size_t s_cleans = 0;
static size_t s_webui_results = 0;
static size_t s_webui_frees = 0;

void bridge_mark_dirty(void)
{
    s_dirty = 1;
}

/* Rule 3: every bound call is answered later from the main thread, so WebUI must run its
 * callbacks in asynchronous_response mode. WebUI resets its config on clean. */
int bridge_ensure_async(void)
{
    if (!s_async)
    {
        webui_set_config(BRIDGE_CFG_ASYNCHRONOUS_RESPONSE, 1);
        bridge_mark_dirty();
        s_async = 1;
    }
    return 1;
}

void bridge_ret_size(size_t v)
{
    double d;

    if (!bridge_size2n(v, &d))
    {
        d = 0.0;
    }
    hb_retndlen(d, 0, 0);
}

/* WebUI refuses new windows for the rest of the process once webui_exit() has run, until
 * webui_clean() resets it. The bridge turns that silent 0 into an error. */
int bridge_new_window(size_t *window)
{
    if (s_exited)
    {
        bridge_raise("WEBUI_NEW_WINDOW: the application was exited; call WEBUI_CLEAN() first");
        return 0;
    }
    bridge_ensure_async();
    *window = webui_new_window();
    bridge_mark_dirty();
    return 1;
}

void bridge_mark_exited(void)
{
    bridge_mark_dirty();
    s_exited = 1;
}

int bridge_is_exited(void)
{
    return s_exited;
}

void bridge_mark_shown(void)
{
    s_shown = 1;
}

int bridge_is_shown(void)
{
    return s_shown;
}

void bridge_note_webui_result(void)
{
    s_webui_results++;
}

void bridge_note_webui_free(void)
{
    s_webui_frees++;
}

/* ---- handler table keyed by bind id ------------------------------------------------- */

struct tab_entry
{
    char *name;
    size_t window;
};

static struct tab_entry *s_tab = NULL;
static size_t s_tab_cap = 0;
static size_t s_tab_count = 0;

int bridge_table_set(size_t bind_id, size_t window, const char *name)
{
    char *copy;
    struct tab_entry *grown;
    size_t cap;
    size_t i;

    if (bind_id == 0 || bind_id > BRIDGE_ID_MAX)
    {
        return 0;
    }
    if (bind_id >= s_tab_cap)
    {
        cap = s_tab_cap == 0 ? 16 : s_tab_cap;
        while (cap <= bind_id)
        {
            cap *= 2;
        }
        grown = (struct tab_entry *) bridge_malloc(cap * sizeof(struct tab_entry));
        if (grown == NULL)
        {
            return 0;
        }
        for (i = 0; i < cap; i++)
        {
            if (i < s_tab_cap)
            {
                grown[i] = s_tab[i];
            }
            else
            {
                grown[i].name = NULL;
                grown[i].window = 0;
            }
        }
        bridge_free(s_tab);
        s_tab = grown;
        s_tab_cap = cap;
    }
    copy = bridge_strdup(name);
    if (copy == NULL)
    {
        return 0;
    }
    if (s_tab[bind_id].name != NULL)
    {
        bridge_free(s_tab[bind_id].name);
    }
    else
    {
        s_tab_count++;
    }
    s_tab[bind_id].name = copy;
    s_tab[bind_id].window = window;
    return 1;
}

const char *bridge_table_get(size_t bind_id)
{
    if (bind_id == 0 || bind_id >= s_tab_cap)
    {
        return NULL;
    }
    return s_tab[bind_id].name;
}

size_t bridge_table_count(void)
{
    return s_tab_count;
}

void bridge_table_clear(void)
{
    size_t i;

    for (i = 0; i < s_tab_cap; i++)
    {
        if (s_tab[i].name != NULL)
        {
            bridge_free(s_tab[i].name);
        }
    }
    bridge_free(s_tab);
    s_tab = NULL;
    s_tab_cap = 0;
    s_tab_count = 0;
}

static size_t bind_common(size_t window, const char *element, const char *handler, int iface)
{
    char upper[BRIDGE_HANDLER_NAME_MAX + 1];
    size_t len;
    size_t i;
    size_t id;
    void *dyn;

    if (element == NULL || handler == NULL)
    {
        return 0;
    }
    len = strlen(handler);
    if (len == 0 || len > BRIDGE_HANDLER_NAME_MAX)
    {
        return 0;
    }
    for (i = 0; i < len; i++)
    {
        char c = handler[i];
        upper[i] = (char) ((c >= 'a' && c <= 'z') ? (c - 'a' + 'A') : c);
    }
    upper[len] = '\0';
    dyn = hb_dynsymFind(upper);
    if (dyn == NULL || !hb_dynsymIsFunction(dyn))
    {
        return 0;
    }
    bridge_ensure_async();
    if (iface)
    {
        id = webui_interface_bind(window, element, bridge_on_iface_event);
    }
    else
    {
        id = webui_bind(window, element, bridge_on_event);
    }
    if (id == 0)
    {
        return 0;
    }
    bridge_mark_dirty();
    if (!bridge_table_set(id, window, upper))
    {
        return 0;
    }
    return id;
}

size_t bridge_bind(size_t window, const char *element, const char *handler)
{
    return bind_common(window, element, handler, 0);
}

/* ---- context slot table (rule 11) --------------------------------------------------- */

/* The Harbour value is cloned into bridge storage; WebUI only ever sees the slot number
 * (slot = index + 1, 0 means none) as its void* context. A free slot has item == NULL. */
struct ctx_slot
{
    size_t window;
    char *element;
    void *item;
};

static struct ctx_slot *s_ctx = NULL;
static size_t s_ctx_cap = 0;
static size_t s_ctx_live = 0;

static size_t ctx_find(size_t window, const char *element)
{
    size_t i;

    for (i = 0; i < s_ctx_cap; i++)
    {
        if (s_ctx[i].item != NULL && s_ctx[i].window == window && strcmp(s_ctx[i].element, element) == 0)
        {
            return i + 1;
        }
    }
    return 0;
}

static void ctx_release(size_t index)
{
    hb_itemRelease(s_ctx[index].item);
    bridge_free(s_ctx[index].element);
    s_ctx[index].item = NULL;
    s_ctx[index].element = NULL;
    s_ctx[index].window = 0;
    s_ctx_live--;
}

/* Returns a free slot number, growing the table when needed; 0 if out of memory or full. */
static size_t ctx_alloc(void)
{
    struct ctx_slot *grown;
    size_t cap;
    size_t i;

    for (i = 0; i < s_ctx_cap; i++)
    {
        if (s_ctx[i].item == NULL)
        {
            return i + 1;
        }
    }
    if (s_ctx_cap >= BRIDGE_ID_MAX)
    {
        return 0;
    }
    cap = s_ctx_cap == 0 ? 8 : s_ctx_cap * 2;
    grown = (struct ctx_slot *) bridge_malloc(cap * sizeof(struct ctx_slot));
    if (grown == NULL)
    {
        return 0;
    }
    for (i = 0; i < cap; i++)
    {
        if (i < s_ctx_cap)
        {
            grown[i] = s_ctx[i];
        }
        else
        {
            grown[i].window = 0;
            grown[i].element = NULL;
            grown[i].item = NULL;
        }
    }
    bridge_free(s_ctx);
    i = s_ctx_cap;
    s_ctx = grown;
    s_ctx_cap = cap;
    return i + 1;
}

void *bridge_context_value(size_t slot)
{
    if (slot == 0 || slot > s_ctx_cap || s_ctx[slot - 1].item == NULL)
    {
        return NULL;
    }
    return hb_itemClone(s_ctx[slot - 1].item);
}

static void bridge_context_clear(void)
{
    size_t i;

    for (i = 0; i < s_ctx_cap; i++)
    {
        if (s_ctx[i].item != NULL)
        {
            ctx_release(i);
        }
    }
    bridge_free(s_ctx);
    s_ctx = NULL;
    s_ctx_cap = 0;
}

/* ---- argument helpers and rule 9 ranges --------------------------------------------- */

int bridge_raise(const char *what)
{
    hb_errRT_BASE(HB_EG_ARG, HB_ERR_SUB_ARGS, what, HB_ERR_FUNCNAME_PTR, HB_ERR_ARGS_BASE);
    return 0;
}

int bridge_range_ok(size_t v, size_t lo, size_t hi)
{
    return v >= lo && v <= hi;
}

int bridge_range_window_size(size_t w, size_t h)
{
    return bridge_range_ok(w, BRIDGE_WIN_W_MIN, BRIDGE_WIN_W_MAX) &&
           bridge_range_ok(h, BRIDGE_WIN_H_MIN, BRIDGE_WIN_H_MAX);
}

int bridge_range_window_pos(size_t x, size_t y)
{
    return bridge_range_ok(x, 0, BRIDGE_POS_X_MAX) && bridge_range_ok(y, 0, BRIDGE_POS_Y_MAX);
}

/* 0 would mean "wait forever" in WebUI; the bridge refuses it (rule 4 and 9). */
int bridge_range_script_timeout(size_t seconds)
{
    return bridge_range_ok(seconds, 1, BRIDGE_SCRIPT_TIMEOUT_MAX);
}

int bridge_arg_size(int iParam, size_t lo, size_t hi, const char *what, size_t *out)
{
    char msg[128];
    size_t v;

    if (hb_param(iParam, BRIDGE_HB_IT_NUMERIC) == NULL)
    {
        sprintf(msg, "%.64s: a number is required", what);
        return bridge_raise(msg);
    }
    if (!bridge_n2size(hb_parnd(iParam), &v))
    {
        sprintf(msg, "%.64s: not an integer between 0 and 2^53", what);
        return bridge_raise(msg);
    }
    if (!bridge_range_ok(v, lo, hi))
    {
        sprintf(msg, "%.64s: out of range %lu..%lu", what, (unsigned long) lo, (unsigned long) hi);
        return bridge_raise(msg);
    }
    *out = v;
    return 1;
}

int bridge_arg_window(int iParam, size_t *out)
{
    return bridge_arg_size(iParam, 1, BRIDGE_ID_MAX, "window id", out);
}

int bridge_arg_bool(int iParam, int *out)
{
    if (hb_param(iParam, BRIDGE_HB_IT_LOGICAL) == NULL)
    {
        return bridge_raise("a logical value is required");
    }
    *out = hb_parl(iParam) ? 1 : 0;
    return 1;
}

int bridge_arg_string(int iParam, const char *what, const char **out)
{
    char msg[128];
    const char *s;

    if (hb_param(iParam, BRIDGE_HB_IT_STRING) == NULL)
    {
        sprintf(msg, "%.64s: a string is required", what);
        return bridge_raise(msg);
    }
    s = hb_parc(iParam);
    if (s == NULL || strlen(s) != hb_parclen(iParam))
    {
        sprintf(msg, "%.64s: must not contain a NUL byte", what);
        return bridge_raise(msg);
    }
    *out = s;
    return 1;
}

/* ---- W1 Harbour functions ------------------------------------------------------------ */

/* WEBUI_BIND( nWindow, cElement, cHandler ) -> nBindId. cHandler names a Harbour FUNCTION. */
void HB_FUN_WEBUI_BIND(void)
{
    size_t window;
    size_t id;
    const char *element;
    const char *handler;

    if (!bridge_arg_window(1, &window) || !bridge_arg_string(2, "element", &element) ||
        !bridge_arg_string(3, "handler", &handler))
    {
        return;
    }
    if (strlen(handler) == 0 || strlen(handler) > BRIDGE_HANDLER_NAME_MAX)
    {
        bridge_raise("handler: a function name of 1 to 64 characters is required");
        return;
    }
    id = bridge_bind(window, element, handler);
    if (id == 0)
    {
        bridge_raise("handler: not a Harbour function (add REQUEST) or bind failed");
        return;
    }
    bridge_ret_size(id);
}

/* WEBUI_INTERFACE_BIND( nWindow, cElement, cHandler ) -> nBindId. Same handler convention
 * as WEBUI_BIND (a Harbour FUNCTION called as handler(cArg, nWindow, nBind, nEvent, nType)),
 * but WebUI binds it through webui_interface_bind. Inside the handler the interface
 * functions (explicit window and event number) and the current-event functions both work. */
void HB_FUN_WEBUI_INTERFACE_BIND(void)
{
    size_t window;
    size_t id;
    const char *element;
    const char *handler;

    if (!bridge_arg_window(1, &window) || !bridge_arg_string(2, "element", &element) ||
        !bridge_arg_string(3, "handler", &handler))
    {
        return;
    }
    if (strlen(handler) == 0 || strlen(handler) > BRIDGE_HANDLER_NAME_MAX)
    {
        bridge_raise("handler: a function name of 1 to 64 characters is required");
        return;
    }
    id = bind_common(window, element, handler, 1);
    if (id == 0)
    {
        bridge_raise("handler: not a Harbour function (add REQUEST) or interface bind failed");
        return;
    }
    bridge_ret_size(id);
}

/* WEBUI_SET_CONTEXT( nWindow, cElement, xValue ) -> nBindId. Rule 11. The value (NIL clears)
 * is cloned into the bridge slot table; WebUI stores only the slot number. A new value for
 * the same window and element releases the old one. Side effect of webui_set_context:
 * webui_bind(window, element, NULL) registers the element (returned as nBindId; a later
 * WEBUI_BIND of the same element returns the same id and keeps the context). */
void HB_FUN_WEBUI_SET_CONTEXT(void)
{
    size_t window;
    size_t id;
    size_t slot;
    const char *element;
    void *item;
    void *copy;
    char *name;

    if (!bridge_arg_window(1, &window) || !bridge_arg_string(2, "element", &element))
    {
        return;
    }
    if (element[0] == '\0')
    {
        bridge_raise("element: must not be empty");
        return;
    }
    if (hb_pcount() < 3)
    {
        bridge_raise("context: a value (or NIL) is required");
        return;
    }
    bridge_mark_dirty();
    id = webui_bind(window, element, NULL);
    if (id == 0)
    {
        bridge_raise("window: does not exist or no bind id is free");
        return;
    }
    item = hb_param(3, BRIDGE_HB_IT_ANY);
    slot = ctx_find(window, element);
    if (item == NULL || hb_itemType(item) == 0)
    {
        if (slot != 0)
        {
            ctx_release(slot - 1);
        }
        webui_set_context(window, element, NULL);
        bridge_ret_size(id);
        return;
    }
    copy = hb_itemClone(item);
    if (slot != 0)
    {
        hb_itemRelease(s_ctx[slot - 1].item);
        s_ctx[slot - 1].item = copy;
    }
    else
    {
        name = bridge_strdup(element);
        slot = name != NULL ? ctx_alloc() : 0;
        if (slot == 0)
        {
            bridge_free(name);
            hb_itemRelease(copy);
            bridge_raise("context: out of memory or no free slot");
            return;
        }
        s_ctx[slot - 1].window = window;
        s_ctx[slot - 1].element = name;
        s_ctx[slot - 1].item = copy;
        s_ctx_live++;
    }
    webui_set_context(window, element, (void *) slot);
    bridge_ret_size(id);
}

/* WEBUI_SET_TIMEOUT( nSeconds ): 0 (wait forever) to 60, rule 9. */
void HB_FUN_WEBUI_SET_TIMEOUT(void)
{
    size_t sec;

    if (!bridge_arg_size(1, 0, BRIDGE_TIMEOUT_MAX, "timeout", &sec))
    {
        return;
    }
    bridge_mark_dirty();
    webui_set_timeout(sec);
    hb_ret();
}

/* WEBUI_SET_CONFIG( nOption, lStatus ). asynchronous_response is owned by the bridge
 * (rule 3) and cannot be changed from Harbour. */
void HB_FUN_WEBUI_SET_CONFIG(void)
{
    size_t option;
    int status;

    if (!bridge_arg_size(1, 0, BRIDGE_CFG_ASYNCHRONOUS_RESPONSE, "option", &option) ||
        !bridge_arg_bool(2, &status))
    {
        return;
    }
    if (option == BRIDGE_CFG_ASYNCHRONOUS_RESPONSE)
    {
        bridge_raise("option: asynchronous_response is managed by the bridge");
        return;
    }
    bridge_mark_dirty();
    webui_set_config((int) option, status);
    hb_ret();
}

/* WEBUI_WAIT(): blocks until all windows are closed. Rule 3: the wait is a loop of
 * non-blocking waits so that bound calls keep being dispatched on this thread. */
void HB_FUN_WEBUI_WAIT(void)
{
    if (bridge_in_dispatch())
    {
        bridge_raise("WEBUI_WAIT must not be called from a bound handler");
        return;
    }
    bridge_mark_dirty();
    for (;;)
    {
        bridge_pump();
        if (!webui_wait_async())
        {
            break;
        }
        hb_idleSleep(0.002);
    }
    bridge_queue_discard();
    hb_ret();
}

/* WEBUI_WAIT_ASYNC() -> lRunning. One non-blocking step; dispatches queued calls. */
void HB_FUN_WEBUI_WAIT_ASYNC(void)
{
    int running;

    bridge_mark_dirty();
    bridge_pump();
    running = webui_wait_async() ? 1 : 0;
    if (!running)
    {
        bridge_queue_discard();
    }
    hb_retl(running);
}

/* WEBUI_EXIT(): closes every window; WEBUI_WAIT returns. Allowed inside a handler, where
 * it first answers the open call with an empty string (see bridge_answer_all). */
void HB_FUN_WEBUI_EXIT(void)
{
    bridge_mark_exited();
    bridge_answer_all();
    webui_exit();
    hb_ret();
}

/* WEBUI_CLEAN() -> lCleaned. Rule 5: inside a handler it only exits (and records that a
 * clean is owed); call it again after WEBUI_WAIT has returned. Without any WebUI use
 * since the last clean it does nothing. */
void HB_FUN_WEBUI_CLEAN(void)
{
    if (bridge_in_dispatch())
    {
        bridge_mark_exited();
        bridge_answer_all();
        webui_exit();
        s_clean_pending = 1;
        hb_retl(0);
        return;
    }
    if (!s_dirty)
    {
        s_clean_pending = 0;
        hb_retl(0);
        return;
    }
    bridge_answer_all();
    webui_clean();
    bridge_table_clear();
    bridge_context_clear();
    s_async = 0;
    s_dirty = 0;
    s_exited = 0;
    s_shown = 0;
    s_clean_pending = 0;
    s_cleans++;
    hb_retl(1);
}

/* WEBUI_BRIDGE_STAT( cName ) -> nValue. Counters for tests and leak checks. */
void HB_FUN_WEBUI_BRIDGE_STAT(void)
{
    const char *name;
    size_t v;

    if (!bridge_arg_string(1, "statistic", &name))
    {
        return;
    }
    if (strcmp(name, "binds") == 0)
    {
        v = bridge_table_count();
    }
    else if (strcmp(name, "queued") == 0)
    {
        v = bridge_queue_length();
    }
    else if (strcmp(name, "dispatched") == 0)
    {
        v = bridge_event_stat(1);
    }
    else if (strcmp(name, "auto_responses") == 0)
    {
        v = bridge_event_stat(2);
    }
    else if (strcmp(name, "missing_handlers") == 0)
    {
        v = bridge_event_stat(3);
    }
    else if (strcmp(name, "allocs") == 0 || strcmp(name, "frees") == 0 || strcmp(name, "live_bytes") == 0)
    {
        bridge_lock();
        v = name[0] == 'a' ? s_allocs : (name[0] == 'f' ? s_frees : s_live_bytes);
        bridge_unlock();
    }
    else if (strcmp(name, "cleans") == 0)
    {
        v = s_cleans;
    }
    else if (strcmp(name, "clean_pending") == 0)
    {
        v = (size_t) s_clean_pending;
    }
    else if (strcmp(name, "dirty") == 0)
    {
        v = (size_t) s_dirty;
    }
    else if (strcmp(name, "exited") == 0)
    {
        v = (size_t) s_exited;
    }
    else if (strcmp(name, "shown") == 0)
    {
        v = (size_t) s_shown;
    }
    else if (strcmp(name, "contexts") == 0)
    {
        v = s_ctx_live;
    }
    else if (strcmp(name, "webui_results") == 0)
    {
        v = s_webui_results;
    }
    else if (strcmp(name, "webui_frees") == 0)
    {
        v = s_webui_frees;
    }
    else
    {
        bridge_raise("statistic: unknown name");
        return;
    }
    bridge_ret_size(v);
}

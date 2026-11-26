/*
 * bridge_event.c  --  bind trampoline, main-thread event queue, dispatcher (ISO C89).
 *
 * Spec 2.1 rule 3. WebUI runs in asynchronous_response mode (bridge_ensure_async): after
 * the callback returns WebUI keeps the browser call open until webui_return_string() is
 * called, and the event pointer stays valid until then. The trampoline therefore only
 * queues the event. bridge_pump() runs on the main thread (WEBUI_PUMP, WEBUI_WAIT,
 * WEBUI_WAIT_ASYNC), calls the Harbour handler registered for the bind id, and answers
 * the browser. No Harbour VM call happens on a WebUI thread.
 *
 * An event still queued when the application exits is dropped without an answer: WebUI
 * ignores responses to abandoned events and keeps the event memory itself.
 */

#include <string.h>

#include "bridge_int.h"

static struct bridge_event *s_head = NULL;
static struct bridge_event *s_tail = NULL;
static size_t s_queued = 0;

static struct bridge_event *s_cur = NULL;
static int s_cur_responded = 0;
static int s_pumping = 0;

static size_t s_dispatched = 0;
static size_t s_auto = 0;
static size_t s_missing = 0;

/* Runs on a WebUI thread. */
void bridge_on_event(webui_event_t *e)
{
    struct bridge_event *ev;

    ev = (struct bridge_event *) bridge_malloc(sizeof(struct bridge_event));
    if (ev == NULL)
    {
        webui_return_string(e, "");
        return;
    }
    ev->next = NULL;
    ev->e = e;
    ev->window = e->window;
    ev->event_type = e->event_type;
    ev->event_number = e->event_number;
    ev->bind_id = e->bind_id;
    bridge_lock();
    if (s_tail == NULL)
    {
        s_head = ev;
    }
    else
    {
        s_tail->next = ev;
    }
    s_tail = ev;
    s_queued++;
    bridge_unlock();
}

static struct bridge_event *queue_take(void)
{
    struct bridge_event *ev;

    bridge_lock();
    ev = s_head;
    if (ev != NULL)
    {
        s_head = ev->next;
        if (s_head == NULL)
        {
            s_tail = NULL;
        }
        s_queued--;
    }
    bridge_unlock();
    return ev;
}

size_t bridge_queue_length(void)
{
    size_t n;

    bridge_lock();
    n = s_queued;
    bridge_unlock();
    return n;
}

void bridge_queue_discard(void)
{
    struct bridge_event *ev;

    while ((ev = queue_take()) != NULL)
    {
        bridge_free(ev);
    }
}

int bridge_in_dispatch(void)
{
    return s_cur != NULL;
}

size_t bridge_event_stat(int which)
{
    switch (which)
    {
    case 1:
        return s_dispatched;
    case 2:
        return s_auto;
    case 3:
        return s_missing;
    default:
        return 0;
    }
}

static void call_handler(struct bridge_event *ev, const char *arg)
{
    char name[BRIDGE_HANDLER_NAME_MAX + 1];
    const char *tabname;
    void *dyn;
    void *ret;

    tabname = bridge_table_get(ev->bind_id);
    if (tabname == NULL)
    {
        s_missing++;
        return;
    }
    strcpy(name, tabname);
    dyn = hb_dynsymFind(name);
    if (dyn == NULL || !hb_dynsymIsFunction(dyn))
    {
        s_missing++;
        webui_return_string(ev->e, "ERROR: Harbour handler not found");
        s_cur_responded = 1;
        return;
    }
    hb_vmPushDynSym(dyn);
    hb_vmPushNil();
    hb_vmPushString(arg, strlen(arg));
    hb_vmPushInteger((int) ev->window);
    hb_vmPushInteger((int) ev->bind_id);
    hb_vmPushDouble((double) ev->event_number, 0);
    hb_vmPushInteger((int) ev->event_type);
    hb_vmDo(5);
    if (!s_cur_responded)
    {
        ret = hb_stackReturnItem();
        if (ret != NULL && (hb_itemType(ret) & BRIDGE_HB_IT_STRING) != 0)
        {
            webui_return_string(ev->e, hb_itemGetCPtr(ret));
            s_cur_responded = 1;
        }
    }
}

/* Dispatches every queued call. Returns how many were dispatched. A handler that calls
 * WEBUI_PUMP or WEBUI_WAIT_ASYNC does not re-enter (those calls dispatch nothing). */
int bridge_pump(void)
{
    struct bridge_event *ev;
    const char *arg;
    int n = 0;

    if (s_pumping)
    {
        return 0;
    }
    s_pumping = 1;
    while ((ev = queue_take()) != NULL)
    {
        arg = webui_get_string(ev->e);
        if (arg == NULL)
        {
            arg = "";
        }
        s_cur = ev;
        s_cur_responded = 0;
        call_handler(ev, arg);
        if (!s_cur_responded)
        {
            webui_return_string(ev->e, "");
            s_auto++;
        }
        s_cur = NULL;
        s_dispatched++;
        n++;
        bridge_free(ev);
    }
    s_pumping = 0;
    return n;
}

/* WEBUI_PUMP() -> nDispatched. Main thread only; a no-op when called from a handler. */
void HB_FUN_WEBUI_PUMP(void)
{
    hb_retni(bridge_pump());
}

/* WEBUI_GET_STRING() -> cArgument. Valid only inside a bound handler; it is the first
 * argument of the JavaScript call, the same text the handler receives as parameter 1. */
void HB_FUN_WEBUI_GET_STRING(void)
{
    const char *s;

    if (s_cur == NULL)
    {
        bridge_raise("WEBUI_GET_STRING is valid only inside a bound handler");
        return;
    }
    s = webui_get_string(s_cur->e);
    hb_retc(s != NULL ? s : "");
}

/* WEBUI_RETURN_STRING( cText ) -> lAccepted. Valid only inside a bound handler. The first
 * answer wins (WebUI ignores later ones); the handler's own RETURN value is used only if
 * no answer was given. */
void HB_FUN_WEBUI_RETURN_STRING(void)
{
    const char *text;

    if (s_cur == NULL)
    {
        bridge_raise("WEBUI_RETURN_STRING is valid only inside a bound handler");
        return;
    }
    if (!bridge_arg_string(1, "text", &text))
    {
        return;
    }
    if (s_cur_responded)
    {
        hb_retl(0);
        return;
    }
    webui_return_string(s_cur->e, text);
    s_cur_responded = 1;
    hb_retl(1);
}

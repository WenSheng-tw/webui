/*
 * bridge_event.c  --  bind trampolines, main-thread event queue, dispatcher (ISO C89).
 *
 * Spec 2.1 rule 3. WebUI runs in asynchronous_response mode (bridge_ensure_async): after
 * the callback returns WebUI keeps the browser call open until a response is set. The
 * trampolines therefore only queue the call (window, event number, element, bind id).
 * bridge_pump() runs on the main thread (WEBUI_PUMP, WEBUI_WAIT, WEBUI_WAIT_ASYNC), calls the
 * Harbour handler registered for the bind id, and answers the browser. No Harbour VM call
 * happens on a WebUI thread.
 *
 * Two trampolines exist: bridge_on_event for WEBUI_BIND (webui_bind) and
 * bridge_on_iface_event for WEBUI_INTERFACE_BIND (webui_interface_bind). The dispatcher does
 * not care which one queued the call. It never keeps WebUI's event pointer: every WebUI call
 * about an event gets a temporary webui_event_t holding only window, event number and
 * element, and WebUI validates the event number (a finished or unknown event is ignored).
 *
 * An event still queued when the application exits is answered with an empty string first
 * (bridge_answer_all): WebUI keeps the memory of an unanswered event when it exits.
 */

#include <stdio.h>
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

static void ev_tmp(const struct bridge_event *ev, webui_event_t *tmp)
{
    memset(tmp, 0, sizeof(webui_event_t));
    tmp->window = ev->window;
    tmp->event_type = ev->event_type;
    tmp->element = ev->element;
    tmp->event_number = ev->event_number;
    tmp->bind_id = ev->bind_id;
}

static void ev_free(struct bridge_event *ev)
{
    if (ev != NULL)
    {
        bridge_free(ev->element);
        bridge_free(ev);
    }
}

static void respond(const struct bridge_event *ev, const char *text)
{
    webui_event_t tmp;

    ev_tmp(ev, &tmp);
    webui_return_string(&tmp, text);
}

/* Runs on a WebUI thread. */
static void queue_event(size_t window, size_t event_type, const char *element, size_t event_number,
                        size_t bind_id)
{
    struct bridge_event *ev;
    webui_event_t tmp;

    ev = (struct bridge_event *) bridge_malloc(sizeof(struct bridge_event));
    if (ev != NULL)
    {
        ev->element = bridge_strdup(element != NULL ? element : "");
        if (ev->element == NULL)
        {
            bridge_free(ev);
            ev = NULL;
        }
    }
    if (ev == NULL)
    {
        memset(&tmp, 0, sizeof(tmp));
        tmp.window = window;
        tmp.event_number = event_number;
        webui_return_string(&tmp, "");
        return;
    }
    ev->next = NULL;
    ev->window = window;
    ev->event_type = event_type;
    ev->event_number = event_number;
    ev->bind_id = bind_id;
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

void bridge_on_event(webui_event_t *e)
{
    queue_event(e->window, e->event_type, e->element, e->event_number, e->bind_id);
}

void bridge_on_iface_event(size_t window, size_t event_type, char *element, size_t event_number,
                           size_t bind_id)
{
    queue_event(window, event_type, element, event_number, bind_id);
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
        ev_free(ev);
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

int bridge_cur_event(const char *fn, webui_event_t *tmp)
{
    char msg[128];

    if (s_cur == NULL)
    {
        sprintf(msg, "%.60s is valid only inside a bound handler", fn);
        return bridge_raise(msg);
    }
    ev_tmp(s_cur, tmp);
    return 1;
}

int bridge_claim_answer(void)
{
    if (s_cur == NULL || s_cur_responded)
    {
        return 0;
    }
    s_cur_responded = 1;
    return 1;
}

int bridge_claim_event(size_t window, size_t event_number)
{
    if (s_cur != NULL && s_cur->window == window && s_cur->event_number == event_number)
    {
        return bridge_claim_answer();
    }
    return 1;
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
        respond(ev, "ERROR: Harbour handler not found");
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
            respond(ev, hb_itemGetCPtr(ret));
            s_cur_responded = 1;
        }
    }
}

/* Answers the call being dispatched (if still open) and every queued call with an empty
 * string. WebUI keeps the memory of an event that is still unanswered when the application
 * exits (by design, the late response is ignored), so WEBUI_EXIT and WEBUI_CLEAN call this
 * first. Calls that arrive after the exit began cannot be helped. */
void bridge_answer_all(void)
{
    struct bridge_event *ev;

    if (s_cur != NULL && !s_cur_responded)
    {
        respond(s_cur, "");
        s_cur_responded = 1;
        s_auto++;
    }
    while ((ev = queue_take()) != NULL)
    {
        respond(ev, "");
        s_auto++;
        ev_free(ev);
    }
}

/* Dispatches every queued call. Returns how many were dispatched. A handler that calls
 * WEBUI_PUMP or WEBUI_WAIT_ASYNC does not re-enter (those calls dispatch nothing). */
int bridge_pump(void)
{
    struct bridge_event *ev;
    webui_event_t tmp;
    const char *arg;
    int n = 0;

    if (s_pumping)
    {
        return 0;
    }
    s_pumping = 1;
    while ((ev = queue_take()) != NULL)
    {
        ev_tmp(ev, &tmp);
        arg = webui_get_string(&tmp);
        if (arg == NULL)
        {
            arg = "";
        }
        s_cur = ev;
        s_cur_responded = 0;
        call_handler(ev, arg);
        if (!s_cur_responded)
        {
            respond(ev, "");
            s_auto++;
        }
        s_cur = NULL;
        s_dispatched++;
        n++;
        ev_free(ev);
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
    webui_event_t tmp;
    const char *s;

    if (!bridge_cur_event("WEBUI_GET_STRING", &tmp))
    {
        return;
    }
    s = webui_get_string(&tmp);
    hb_retc(s != NULL ? s : "");
}

/* WEBUI_RETURN_STRING( cText ) -> lAccepted. Valid only inside a bound handler. The first
 * answer wins (WebUI ignores later ones); the handler's own RETURN value is used only if
 * no answer was given. */
void HB_FUN_WEBUI_RETURN_STRING(void)
{
    webui_event_t tmp;
    const char *text;

    if (!bridge_cur_event("WEBUI_RETURN_STRING", &tmp) || !bridge_arg_string(1, "text", &text))
    {
        return;
    }
    if (!bridge_claim_answer())
    {
        hb_retl(0);
        return;
    }
    webui_return_string(&tmp, text);
    hb_retl(1);
}

/* WEBUI_INTERFACE_SET_RESPONSE( nWindow, nEvent, cText ) -> lSent. Works on any event of a
 * window by number (the handler receives both as parameters 2 and 4). Returns .F. only when
 * the event is the one being dispatched and it was answered already; for another event
 * number the call is passed to WebUI, which ignores finished and unknown events. */
void HB_FUN_WEBUI_INTERFACE_SET_RESPONSE(void)
{
    size_t window;
    size_t number;
    const char *text;

    if (!bridge_arg_window(1, &window) || !bridge_arg_size(2, 0, (size_t) -1, "event number", &number) ||
        !bridge_arg_string(3, "response", &text))
    {
        return;
    }
    if (!bridge_claim_event(window, number))
    {
        hb_retl(0);
        return;
    }
    webui_interface_set_response(window, number, text);
    hb_retl(1);
}

/*
 * bridge_misc.c  --  W2 Harbour functions (ISO C89).
 *
 * Event functions (no event argument): WEBUI_GET_COUNT / GET_*_AT / GET_* / RETURN_INT /
 * RETURN_FLOAT / RETURN_BOOL / GET_CONTEXT act on the event being dispatched and are valid
 * only inside a bound handler (WEBUI_BIND or WEBUI_INTERFACE_BIND). The interface functions
 * take the window and event number (handler parameters 2 and 4) explicitly.
 *
 * Memory (spec 3): strings returned by WebUI (get_string_at, last_error_message and the
 * interface getters) belong to WebUI and are copied at once with hb_retc. The results of
 * webui_encode and webui_decode belong to the caller: copied with hb_retc, then webui_free.
 * 64-bit integers go through bridge_i64.c (the only C99 unit).
 */

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "bridge_int.h"

static int arg_index(int iParam, size_t *out)
{
    return bridge_arg_size(iParam, 0, BRIDGE_ARG_INDEX_MAX, "argument index", out);
}

static int arg_event(int iParam, size_t *out)
{
    return bridge_arg_size(iParam, 0, (size_t) -1, "event number", out);
}

static void ret_int64(int wide, double num, const char *str)
{
    if (wide)
    {
        hb_retc(str);
    }
    else
    {
        hb_retndlen(num, 0, 0);
    }
}

/* ---- event arguments ---------------------------------------------------------------- */

/* WEBUI_GET_COUNT() -> nArguments of the JavaScript call. */
void HB_FUN_WEBUI_GET_COUNT(void)
{
    webui_event_t tmp;

    if (!bridge_cur_event("WEBUI_GET_COUNT", &tmp))
    {
        return;
    }
    bridge_ret_size(webui_get_count(&tmp));
}

/* WEBUI_GET_INT_AT( nIndex ) -> nValue; a decimal string when the value is beyond +-2^53. */
void HB_FUN_WEBUI_GET_INT_AT(void)
{
    webui_event_t tmp;
    size_t index;
    double num;
    char str[32];
    int wide;

    if (!bridge_cur_event("WEBUI_GET_INT_AT", &tmp) || !arg_index(1, &index))
    {
        return;
    }
    wide = bridge_get_int_at(&tmp, index, &num, str, sizeof(str));
    ret_int64(wide, num, str);
}

void HB_FUN_WEBUI_GET_INT(void)
{
    webui_event_t tmp;
    double num;
    char str[32];
    int wide;

    if (!bridge_cur_event("WEBUI_GET_INT", &tmp))
    {
        return;
    }
    wide = bridge_get_int(&tmp, &num, str, sizeof(str));
    ret_int64(wide, num, str);
}

void HB_FUN_WEBUI_GET_FLOAT_AT(void)
{
    webui_event_t tmp;
    size_t index;

    if (!bridge_cur_event("WEBUI_GET_FLOAT_AT", &tmp) || !arg_index(1, &index))
    {
        return;
    }
    hb_retnd(webui_get_float_at(&tmp, index));
}

void HB_FUN_WEBUI_GET_FLOAT(void)
{
    webui_event_t tmp;

    if (!bridge_cur_event("WEBUI_GET_FLOAT", &tmp))
    {
        return;
    }
    hb_retnd(webui_get_float(&tmp));
}

/* WEBUI_GET_STRING_AT( nIndex ) -> cValue ("" if the argument does not exist). */
void HB_FUN_WEBUI_GET_STRING_AT(void)
{
    webui_event_t tmp;
    size_t index;
    const char *s;

    if (!bridge_cur_event("WEBUI_GET_STRING_AT", &tmp) || !arg_index(1, &index))
    {
        return;
    }
    s = webui_get_string_at(&tmp, index);
    hb_retc(s != NULL ? s : "");
}

void HB_FUN_WEBUI_GET_BOOL_AT(void)
{
    webui_event_t tmp;
    size_t index;
    ubool b;

    if (!bridge_cur_event("WEBUI_GET_BOOL_AT", &tmp) || !arg_index(1, &index))
    {
        return;
    }
    b = webui_get_bool_at(&tmp, index);
    hb_retl(b != 0);
}

void HB_FUN_WEBUI_GET_BOOL(void)
{
    webui_event_t tmp;
    ubool b;

    if (!bridge_cur_event("WEBUI_GET_BOOL", &tmp))
    {
        return;
    }
    b = webui_get_bool(&tmp);
    hb_retl(b != 0);
}

/* WEBUI_GET_SIZE_AT( nIndex ) -> nBytes of the argument. */
void HB_FUN_WEBUI_GET_SIZE_AT(void)
{
    webui_event_t tmp;
    size_t index;

    if (!bridge_cur_event("WEBUI_GET_SIZE_AT", &tmp) || !arg_index(1, &index))
    {
        return;
    }
    bridge_ret_size(webui_get_size_at(&tmp, index));
}

void HB_FUN_WEBUI_GET_SIZE(void)
{
    webui_event_t tmp;

    if (!bridge_cur_event("WEBUI_GET_SIZE", &tmp))
    {
        return;
    }
    bridge_ret_size(webui_get_size(&tmp));
}

/* WEBUI_GET_CONTEXT() -> xValue stored by WEBUI_SET_CONTEXT for the window and element of the
 * current event, a fresh copy of it; NIL if there is none. */
void HB_FUN_WEBUI_GET_CONTEXT(void)
{
    webui_event_t tmp;
    void *slot;
    void *copy;

    if (!bridge_cur_event("WEBUI_GET_CONTEXT", &tmp))
    {
        return;
    }
    slot = webui_get_context(&tmp);
    copy = bridge_context_value((size_t) slot);
    if (copy == NULL)
    {
        hb_ret();
        return;
    }
    hb_itemReturn(copy);
    hb_itemRelease(copy);
}

/* ---- answers ------------------------------------------------------------------------ */

/* WEBUI_RETURN_INT( xValue ) -> lAccepted. xValue is an integer number within +-2^53 or a
 * decimal string within long long ('-' and 1 to 19 digits). Anything else raises. The first
 * answer to the event wins; .F. if it was answered already. */
void HB_FUN_WEBUI_RETURN_INT(void)
{
    webui_event_t tmp;
    const char *text;
    double d;
    int is_text;
    int sent;

    if (!bridge_cur_event("WEBUI_RETURN_INT", &tmp))
    {
        return;
    }
    is_text = hb_param(1, BRIDGE_HB_IT_STRING) != NULL;
    if (is_text)
    {
        if (!bridge_arg_string(1, "value", &text))
        {
            return;
        }
    }
    else
    {
        if (hb_param(1, BRIDGE_HB_IT_NUMERIC) == NULL)
        {
            bridge_raise("value: a number or a decimal string is required");
            return;
        }
        d = hb_parnd(1);
        if (d != d || d < -9007199254740992.0 || d > 9007199254740992.0 || d != floor(d))
        {
            bridge_raise("value: not an integer within +-2^53 (use a decimal string beyond that)");
            return;
        }
    }
    if (!bridge_claim_answer())
    {
        hb_retl(0);
        return;
    }
    sent = is_text ? bridge_return_int_text(&tmp, text) : bridge_return_int(&tmp, d);
    if (!sent)
    {
        bridge_raise("value: not a decimal integer within long long");
        return;
    }
    hb_retl(1);
}

/* WEBUI_RETURN_FLOAT( nValue ) -> lAccepted. NaN and infinity raise. */
void HB_FUN_WEBUI_RETURN_FLOAT(void)
{
    webui_event_t tmp;
    double d;

    if (!bridge_cur_event("WEBUI_RETURN_FLOAT", &tmp))
    {
        return;
    }
    if (hb_param(1, BRIDGE_HB_IT_NUMERIC) == NULL)
    {
        bridge_raise("value: a number is required");
        return;
    }
    d = hb_parnd(1);
    if (d != d || d > DBL_MAX || d < -DBL_MAX)
    {
        bridge_raise("value: NaN and infinity are not allowed");
        return;
    }
    if (!bridge_claim_answer())
    {
        hb_retl(0);
        return;
    }
    webui_return_float(&tmp, d);
    hb_retl(1);
}

/* WEBUI_RETURN_BOOL( lValue ) -> lAccepted. */
void HB_FUN_WEBUI_RETURN_BOOL(void)
{
    webui_event_t tmp;
    int b;

    if (!bridge_cur_event("WEBUI_RETURN_BOOL", &tmp) || !bridge_arg_bool(1, &b))
    {
        return;
    }
    if (!bridge_claim_answer())
    {
        hb_retl(0);
        return;
    }
    webui_return_bool(&tmp, b);
    hb_retl(1);
}

/* ---- encode, decode, free ----------------------------------------------------------- */

static void base64(const char *fn, char *(*convert)(const char *))
{
    const char *in;
    char *out;
    char msg[96];

    if (!bridge_arg_string(1, "text", &in))
    {
        return;
    }
    if (bridge_is_exited())
    {
        sprintf(msg, "%.40s: the application was exited; call WEBUI_CLEAN() first", fn);
        bridge_raise(msg);
        return;
    }
    if (in[0] == '\0')
    {
        hb_retc("");
        return;
    }
    bridge_mark_dirty();
    out = convert(in);
    if (out == NULL)
    {
        sprintf(msg, "%.40s: WebUI could not convert the text (not valid base64?)", fn);
        bridge_raise(msg);
        return;
    }
    bridge_note_webui_result();
    hb_retc(out);
    webui_free(out);
    bridge_note_webui_free();
}

/* WEBUI_ENCODE( cText ) -> cBase64. "" gives "". Raises after WEBUI_EXIT until WEBUI_CLEAN. */
void HB_FUN_WEBUI_ENCODE(void)
{
    base64("WEBUI_ENCODE", webui_encode);
}

/* WEBUI_DECODE( cBase64 ) -> cText. The result is copied as text, so it ends at the first NUL
 * byte of the decoded data. Invalid base64 raises. */
void HB_FUN_WEBUI_DECODE(void)
{
    base64("WEBUI_DECODE", webui_decode);
}

/* WEBUI_FREE( [pPointer] ) -> lCalled. The bridge never hands a WebUI pointer to Harbour (encode
 * and decode free their own results), so there is nothing to free from Harbour: NIL does
 * nothing and returns .F.; a Harbour pointer item is passed to webui_free, which only frees
 * pointers it allocated itself and ignores any other (tested). */
void HB_FUN_WEBUI_FREE(void)
{
    void *p;

    if (hb_pcount() == 0 || hb_param(1, BRIDGE_HB_IT_ANY) == NULL || hb_itemType(hb_param(1, BRIDGE_HB_IT_ANY)) == 0)
    {
        hb_retl(0);
        return;
    }
    if (hb_param(1, BRIDGE_HB_IT_POINTER) == NULL)
    {
        bridge_raise("pointer: a pointer item or NIL is required");
        return;
    }
    p = hb_parptr(1);
    bridge_mark_dirty();
    webui_free(p);
    hb_retl(1);
}

/* ---- last error --------------------------------------------------------------------- */

void HB_FUN_WEBUI_GET_LAST_ERROR_NUMBER(void)
{
    bridge_ret_size(webui_get_last_error_number());
}

void HB_FUN_WEBUI_GET_LAST_ERROR_MESSAGE(void)
{
    const char *s;

    s = webui_get_last_error_message();
    hb_retc(s != NULL ? s : "");
}

/* ---- interface functions ------------------------------------------------------------ */

/* WEBUI_INTERFACE_IS_APP_RUNNING() -> lRunning. Rule 8: WebUI locks the answer to .F. for the
 * rest of the process if it is called before a window was shown, so the bridge raises until
 * WEBUI_SHOW returned .T. once. WebUI also keeps the .F. for good once the wait ended. */
void HB_FUN_WEBUI_INTERFACE_IS_APP_RUNNING(void)
{
    ubool running;

    if (!bridge_is_shown())
    {
        bridge_raise("WEBUI_INTERFACE_IS_APP_RUNNING is valid only after WEBUI_SHOW returned .T. (rule 8)");
        return;
    }
    bridge_mark_dirty();
    running = webui_interface_is_app_running();
    hb_retl(running != 0);
}

/* WEBUI_INTERFACE_GET_WINDOW_ID( nWindow ) -> nWindow, or 0 if there is no such window. */
void HB_FUN_WEBUI_INTERFACE_GET_WINDOW_ID(void)
{
    size_t window;

    if (!bridge_arg_window(1, &window))
    {
        return;
    }
    bridge_mark_dirty();
    bridge_ret_size(webui_interface_get_window_id(window));
}

void HB_FUN_WEBUI_INTERFACE_GET_STRING_AT(void)
{
    size_t window;
    size_t number;
    size_t index;
    const char *s;

    if (!bridge_arg_window(1, &window) || !arg_event(2, &number) || !arg_index(3, &index))
    {
        return;
    }
    s = webui_interface_get_string_at(window, number, index);
    hb_retc(s != NULL ? s : "");
}

void HB_FUN_WEBUI_INTERFACE_GET_INT_AT(void)
{
    size_t window;
    size_t number;
    size_t index;
    double num;
    char str[32];
    int wide;

    if (!bridge_arg_window(1, &window) || !arg_event(2, &number) || !arg_index(3, &index))
    {
        return;
    }
    wide = bridge_interface_get_int_at(window, number, index, &num, str, sizeof(str));
    ret_int64(wide, num, str);
}

void HB_FUN_WEBUI_INTERFACE_GET_FLOAT_AT(void)
{
    size_t window;
    size_t number;
    size_t index;

    if (!bridge_arg_window(1, &window) || !arg_event(2, &number) || !arg_index(3, &index))
    {
        return;
    }
    hb_retnd(webui_interface_get_float_at(window, number, index));
}

void HB_FUN_WEBUI_INTERFACE_GET_BOOL_AT(void)
{
    size_t window;
    size_t number;
    size_t index;
    ubool b;

    if (!bridge_arg_window(1, &window) || !arg_event(2, &number) || !arg_index(3, &index))
    {
        return;
    }
    b = webui_interface_get_bool_at(window, number, index);
    hb_retl(b != 0);
}

void HB_FUN_WEBUI_INTERFACE_GET_SIZE_AT(void)
{
    size_t window;
    size_t number;
    size_t index;

    if (!bridge_arg_window(1, &window) || !arg_event(2, &number) || !arg_index(3, &index))
    {
        return;
    }
    bridge_ret_size(webui_interface_get_size_at(window, number, index));
}

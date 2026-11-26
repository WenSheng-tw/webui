/*
 * bridge.c  --  PoC-001 compatibility names (ISO C89).
 *
 * PoC-001 had a single-file bridge. Since PoC-002 W1 the bridge lives in bridge_core.c,
 * bridge_win.c and bridge_event.c (see bridge_int.h). This file keeps the two names
 * PoC-001's prg/main.prg uses and that have no spec name:
 *
 *   WEBUI_NEWWINDOW()                          same as WEBUI_NEW_WINDOW()
 *   WEBUI_BINDHB( nWindow, cJsName, cFunc )    like WEBUI_BIND, but returns 0 instead of
 *                                              raising an error when anything is wrong
 *
 * WEBUI_SHOW and WEBUI_WAIT keep their PoC-001 names in the new files. Behavior changes
 * against PoC-001 are listed in w1/W1-REPORT.md.
 */

#include "bridge_int.h"

void HB_FUN_WEBUI_NEWWINDOW(void)
{
    bridge_ret_size(bridge_new_window());
}

void HB_FUN_WEBUI_BINDHB(void)
{
    double d;
    size_t window;
    size_t id;
    const char *element;
    const char *handler;

    if (hb_param(1, BRIDGE_HB_IT_NUMERIC) == NULL || hb_param(2, BRIDGE_HB_IT_STRING) == NULL ||
        hb_param(3, BRIDGE_HB_IT_STRING) == NULL)
    {
        hb_retni(0);
        return;
    }
    d = hb_parnd(1);
    if (d < 1.0 || d > (double) BRIDGE_ID_MAX)
    {
        hb_retni(0);
        return;
    }
    window = (size_t) d;
    element = hb_parc(2);
    handler = hb_parc(3);
    id = bridge_bind(window, element, handler);
    bridge_ret_size(id);
}

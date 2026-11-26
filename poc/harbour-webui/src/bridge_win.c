/*
 * bridge_win.c  --  window functions of W1 (ISO C89).
 *
 * Window size and position setters arrive with W2 and use the rule 9 predicates
 * bridge_range_window_size / bridge_range_window_pos from bridge_core.c.
 */

#include "bridge_int.h"

/* WEBUI_NEW_WINDOW() -> nWindow (0 if WebUI could not create one). */
void HB_FUN_WEBUI_NEW_WINDOW(void)
{
    size_t w;

    if (bridge_new_window(&w))
    {
        bridge_ret_size(w);
    }
}

/* WEBUI_SHOW( nWindow, cContent ) -> lShown. cContent is HTML text, a file name or a URL.
 * Rule 8: call it on the main thread; it blocks until the browser connects (default). */
void HB_FUN_WEBUI_SHOW(void)
{
    size_t window;
    const char *content;
    ubool ok;

    if (!bridge_arg_window(1, &window) || !bridge_arg_string(2, "content", &content))
    {
        return;
    }
    bridge_mark_dirty();
    ok = webui_show(window, content);
    if (ok != 0)
    {
        bridge_mark_shown();
    }
    hb_retl(ok != 0);
}

/* WEBUI_CLOSE( nWindow ) */
void HB_FUN_WEBUI_CLOSE(void)
{
    size_t window;

    if (!bridge_arg_window(1, &window))
    {
        return;
    }
    bridge_mark_dirty();
    webui_close(window);
    hb_ret();
}

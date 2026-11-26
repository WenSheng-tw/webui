/*
 * BCC link probe for the WebUI DLL route (ISO C89, no // comments).
 *
 * Purpose: find out whether BCC can resolve webui_new_window and webui_wait
 * from an import library made from webui-2.dll. It does NOT show a window.
 * scripts\probe-bcc-dll.bat only compiles and links this file, it never
 * runs the result. If you run it by hand it creates a window object and
 * calls webui_wait without ever showing a window.
 *
 * Declarations follow the extern style of src/bridge.c; webui.h is not
 * included on purpose.
 */

#include <stddef.h>

extern size_t webui_new_window(void);
extern void webui_wait(void);

int main(void)
{
    size_t window;

    window = webui_new_window();
    if (window == 0)
    {
        return 2;
    }

    webui_wait();
    return 0;
}

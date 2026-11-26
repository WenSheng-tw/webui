/*
 * WebUI <-> Harbour bridge (ISO C89).
 *
 * webui.h and hbapi.h both use C99 constructs (// comments, long long), so
 * this file declares the small subset of both APIs it needs instead of
 * including them. Only int, size_t and void pointers cross the boundary.
 * size_t matches Harbour's HB_SIZE on x32 Windows, x64 Windows and 64-bit Linux.
 */

#include <ctype.h>
#include <stddef.h>
#include <string.h>

/* WebUI subset, see include/webui.h */
typedef struct webui_event_t webui_event_t;

extern size_t webui_new_window(void);
extern size_t webui_bind(size_t window, const char *element, void (*func)(webui_event_t *e));
extern void webui_show(size_t window, const char *content);
extern void webui_set_config(int option, int status);
extern void webui_wait(void);
extern const char *webui_get_string(webui_event_t *e);
extern void webui_return_string(webui_event_t *e, const char *s);

/* Harbour subset, see hbapi.h, hbvm.h, hbapiitm.h, hbstack.h */
extern void *hb_dynsymFind(const char *szName);
extern int hb_dynsymIsFunction(void *pDynSym);
extern void hb_vmPushDynSym(void *pDynSym);
extern void hb_vmPushNil(void);
extern void hb_vmPushString(const char *szText, size_t nLength);
extern void hb_vmDo(unsigned short uiParams);
extern void *hb_stackReturnItem(void);
extern const char *hb_itemGetCPtr(void *pItem);
extern int hb_parni(int iParam);
extern const char *hb_parc(int iParam);
extern void hb_retni(int iNumber);

#define BRIDGE_HANDLER_MAX 64

/* webui_config value ui_event_blocking in include/webui.h */
#define BRIDGE_CFG_UI_EVENT_BLOCKING 1

/* PoC-001 binds a single Harbour handler. */
static char s_szHandler[BRIDGE_HANDLER_MAX];

static void OnBoundCall(webui_event_t *e)
{
    const char *pszArg;
    const char *pszResult;
    void *pDynSym;

    pDynSym = hb_dynsymFind(s_szHandler);
    if (pDynSym == NULL)
    {
        webui_return_string(e, "ERROR: Harbour handler not found");
        return;
    }

    pszArg = webui_get_string(e);
    if (pszArg == NULL)
    {
        pszArg = "";
    }

    hb_vmPushDynSym(pDynSym);
    hb_vmPushNil();
    hb_vmPushString(pszArg, strlen(pszArg));
    hb_vmDo(1);

    pszResult = hb_itemGetCPtr(hb_stackReturnItem());
    webui_return_string(e, pszResult != NULL ? pszResult : "");
}

/* WEBUI_NEWWINDOW() --> nWindow */
void HB_FUN_WEBUI_NEWWINDOW(void)
{
    hb_retni((int) webui_new_window());
}

/* WEBUI_SHOW( nWindow, cHtml ) */
void HB_FUN_WEBUI_SHOW(void)
{
    const char *pszHtml;

    pszHtml = hb_parc(2);
    webui_show((size_t) hb_parni(1), pszHtml != NULL ? pszHtml : "");
}

/* WEBUI_BINDHB( nWindow, cJsName, cHarbourFunc ) --> nBindId or 0 on error */
void HB_FUN_WEBUI_BINDHB(void)
{
    const char *pszJsName;
    const char *pszHandler;
    char szName[BRIDGE_HANDLER_MAX];
    void *pDynSym;
    size_t nLen;
    size_t nBindId;
    size_t i;

    pszJsName = hb_parc(2);
    pszHandler = hb_parc(3);
    if (pszJsName == NULL || pszHandler == NULL)
    {
        hb_retni(0);
        return;
    }

    nLen = strlen(pszHandler);
    if (nLen == 0 || nLen >= BRIDGE_HANDLER_MAX)
    {
        hb_retni(0);
        return;
    }

    for (i = 0; i < nLen; i++)
    {
        szName[i] = (char) toupper((unsigned char) pszHandler[i]);
    }
    szName[nLen] = '\0';

    pDynSym = hb_dynsymFind(szName);
    if (pDynSym == NULL || !hb_dynsymIsFunction(pDynSym))
    {
        hb_retni(0);
        return;
    }

    /*
     * WebUI runs every callback in a new thread by default, but the Harbour
     * VM has a single global stack. Serialize the callbacks. This must be the
     * global option, not webui_set_event_blocking(), which only sets a
     * per-window flag that the receive path does not check.
     */
    webui_set_config(BRIDGE_CFG_UI_EVENT_BLOCKING, 1);

    nBindId = webui_bind((size_t) hb_parni(1), pszJsName, OnBoundCall);
    if (nBindId != 0)
    {
        memcpy(s_szHandler, szName, nLen + 1);
    }
    hb_retni((int) nBindId);
}

/* WEBUI_WAIT() : blocks until all windows are closed */
void HB_FUN_WEBUI_WAIT(void)
{
    webui_wait();
}

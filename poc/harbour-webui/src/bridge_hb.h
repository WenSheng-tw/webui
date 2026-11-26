/*
 * bridge_hb.h  --  hand-written subset of the Harbour C API used by the bridge (ISO C89).
 *
 * Harbour's own headers are not included: they contain C99 constructs and would pull the
 * whole VM header tree into the strict C89 units. Every prototype below is copied from
 * Harbour 3.2.0 (include/hbapi.h, hbvm.h, hbapiitm.h, hbapierr.h, hbapi.h idle section);
 * HB_BOOL is int, HB_USHORT unsigned short, HB_ERRCODE and HB_TYPE unsigned int,
 * HB_ULONG unsigned long, HB_SIZE size_t.
 * PHB_ITEM and PHB_DYNS are passed as opaque void pointers.
 */

#ifndef BRIDGE_HB_H
#define BRIDGE_HB_H

#include <stddef.h>

/* hb_param() masks (hbapi.h) */
#define BRIDGE_HB_IT_INTEGER 0x00002L
#define BRIDGE_HB_IT_LONG 0x00008L
#define BRIDGE_HB_IT_DOUBLE 0x00010L
#define BRIDGE_HB_IT_NUMERIC 0x0001AL
#define BRIDGE_HB_IT_LOGICAL 0x00080L
#define BRIDGE_HB_IT_STRING 0x00400L

extern int hb_pcount(void);
extern void *hb_param(int iParam, long lMask);
extern const char *hb_parc(int iParam);
extern size_t hb_parclen(int iParam);
extern double hb_parnd(int iParam);
extern int hb_parl(int iParam);

extern void hb_ret(void);
extern void hb_retc(const char *szText);
extern void hb_retclen(const char *szText, size_t nLen);
extern void hb_retl(int iTrueFalse);
extern void hb_retnd(double dNumber);
extern void hb_retndlen(double dNumber, int iWidth, int iDec);
extern void hb_retni(int iNumber);

extern unsigned short hb_errRT_BASE(unsigned int errGenCode, unsigned int errSubCode,
                                  const char *szDescription, const char *szOperation,
                                  unsigned long ulArgCount, ...);

extern void *hb_dynsymFind(const char *szName);
extern int hb_dynsymIsFunction(void *pDynSym);
extern void hb_vmPushDynSym(void *pDynSym);
extern void hb_vmPushNil(void);
extern void hb_vmPushInteger(int iNumber);
extern void hb_vmPushDouble(double dNumber, int iDec);
extern void hb_vmPushString(const char *szText, size_t nLen);
extern void hb_vmDo(unsigned short uiParams);
extern void *hb_stackReturnItem(void);

extern const char *hb_itemGetCPtr(void *pItem);
extern size_t hb_itemGetCLen(void *pItem);
extern unsigned int hb_itemType(void *pItem);

extern void hb_idleSleep(double dSeconds);

#endif

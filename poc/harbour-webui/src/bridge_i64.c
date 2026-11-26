/*
 * bridge_i64.c  --  the only C99 unit of the bridge (spec 4).
 *
 * It holds the four WebUI functions that use long long: get_int, get_int_at,
 * interface_get_int_at and return_int. It does not include webui.h or the Harbour headers;
 * it declares the four prototypes itself and turns them into C89-callable helpers whose
 * prototypes are in bridge_int.h. Values travel as double when |v| <= 2^53 and as decimal
 * text beyond that. Build: gcc -std=c99 -pedantic-errors -Wall -Wextra.
 *
 * w0/check_protos.py compares the four declarations below with include/webui.h.
 */

#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct webui_event_t webui_event_t;

extern long long int webui_get_int_at(webui_event_t *e, size_t index);
extern long long int webui_get_int(webui_event_t *e);
extern void webui_return_int(webui_event_t *e, long long int n);
extern long long int webui_interface_get_int_at(size_t window, size_t event_number, size_t index);

#define I64_MAX_EXACT 9007199254740992LL

/* 0: *num holds v exactly. 1: v does not fit, str holds its decimal text. */
static int i64_to_hb(long long v, double *num, char *str, size_t strsize)
{
    if (v >= -I64_MAX_EXACT && v <= I64_MAX_EXACT)
    {
        *num = (double) v;
        if (strsize > 0)
        {
            str[0] = '\0';
        }
        return 0;
    }
    *num = 0.0;
    snprintf(str, strsize, "%lld", v);
    return 1;
}

/* 1 and the exact value if d is an integer within +-2^53, otherwise 0. */
static int hb_to_i64(double d, long long *out)
{
    if (d != d || d < -9007199254740992.0 || d > 9007199254740992.0)
    {
        return 0;
    }
    if (d != (double) (long long) d)
    {
        return 0;
    }
    *out = (long long) d;
    return 1;
}

int bridge_get_int_at(void *e, size_t index, double *num, char *str, size_t strsize)
{
    return i64_to_hb(webui_get_int_at((webui_event_t *) e, index), num, str, strsize);
}

int bridge_get_int(void *e, double *num, char *str, size_t strsize)
{
    return i64_to_hb(webui_get_int((webui_event_t *) e), num, str, strsize);
}

int bridge_interface_get_int_at(size_t window, size_t event_number, size_t index, double *num, char *str,
                                size_t strsize)
{
    return i64_to_hb(webui_interface_get_int_at(window, event_number, index), num, str, strsize);
}

/* 1 if d was accepted and handed to webui_return_int, 0 if rejected (nothing sent). */
int bridge_return_int(void *e, double d)
{
    long long v;

    if (!hb_to_i64(d, &v))
    {
        return 0;
    }
    webui_return_int((webui_event_t *) e, v);
    return 1;
}

/* 1 if s is '-'? followed by 1 to 19 digits and fits long long, 0 otherwise (nothing sent). */
int bridge_return_int_text(void *e, const char *s)
{
    const char *p = s;
    size_t digits = 0;
    long long v;
    char *end;

    if (*p == '-')
    {
        p++;
    }
    while (p[digits] >= '0' && p[digits] <= '9')
    {
        digits++;
    }
    if (digits < 1 || digits > 19 || p[digits] != '\0')
    {
        return 0;
    }
    errno = 0;
    v = strtoll(s, &end, 10);
    if (errno != 0 || *end != '\0')
    {
        return 0;
    }
    webui_return_int((webui_event_t *) e, v);
    return 1;
}

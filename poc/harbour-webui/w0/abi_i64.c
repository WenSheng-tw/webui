/*
 * abi_i64.c  --  the only C99 unit of the bridge side (spec 4, prototype for bridge_i64.c).
 *
 * Does not include webui.h. Declares the four long long WebUI functions itself and turns
 * them into C89-callable helpers: values travel as double when |v| <= 2^53 and as a
 * decimal string beyond that. Build: gcc -std=c99 -pedantic-errors -Wall -Wextra.
 */

#include <limits.h>
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

int bridge_interface_get_int_at(size_t window, size_t event_number, size_t index, double *num, char *str, size_t strsize)
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

/* Test helpers: drive the same paths with exact 64-bit values given as decimal text. */

int abi_i64_convert_decimal(const char *dec, double *num, char *str, size_t strsize)
{
    return i64_to_hb(strtoll(dec, NULL, 10), num, str, strsize);
}

int abi_i64_from_double(double d, char *dec, size_t decsize)
{
    long long v;

    if (!hb_to_i64(d, &v))
    {
        return 0;
    }
    snprintf(dec, decsize, "%lld", v);
    return 1;
}

void abi_i64_return_decimal(void *e, const char *dec)
{
    webui_return_int((webui_event_t *) e, strtoll(dec, NULL, 10));
}

void abi_i64_get_int_at_decimal(void *e, size_t index, char *dec, size_t decsize)
{
    snprintf(dec, decsize, "%lld", webui_get_int_at((webui_event_t *) e, index));
}

void abi_i64_get_int_decimal(void *e, char *dec, size_t decsize)
{
    snprintf(dec, decsize, "%lld", webui_get_int((webui_event_t *) e));
}

void abi_i64_interface_get_int_at_decimal(size_t window, size_t event_number, size_t index, char *dec, size_t decsize)
{
    snprintf(dec, decsize, "%lld", webui_interface_get_int_at(window, event_number, index));
}

size_t abi_i64_sizeof_long_long(void)
{
    return sizeof(long long);
}

void abi_i64_limits(char *min, char *max, size_t size)
{
    snprintf(min, size, "%lld", LLONG_MIN);
    snprintf(max, size, "%lld", LLONG_MAX);
}

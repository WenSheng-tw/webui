/*
 * bridge_num.h  --  double <-> size_t helpers for the bridge (ISO C89, static functions).
 *
 * Harbour numbers cross the boundary as double (hb_parnd / hb_retnd). Every size_t input
 * is range checked here first, never cast directly: negative, NaN, infinite, fractional
 * and values above 2^53 (the last integer a double represents exactly for all smaller
 * ones) are rejected. Spec 2.
 */

#ifndef BRIDGE_NUM_H
#define BRIDGE_NUM_H

#include <stddef.h>

/* 2^53 */
#define BRIDGE_NUM_MAX_EXACT 9007199254740992.0

/* Returns 1 and stores the value on success, 0 if d is not an exact size_t in range. */
static int bridge_n2size(double d, size_t *out)
{
    size_t v;

    if (d != d)
    {
        return 0;
    }
    if (d < 0.0 || d > BRIDGE_NUM_MAX_EXACT)
    {
        return 0;
    }
    /* On 32-bit targets size_t is narrower than 2^53. */
    if (d > (double) ((size_t) -1))
    {
        return 0;
    }
    v = (size_t) d;
    if ((double) v != d)
    {
        return 0;
    }
    *out = v;
    return 1;
}

/* Returns 1 and stores the value on success, 0 if v is above 2^53 and would lose precision. */
static int bridge_size2n(size_t v, double *out)
{
    if ((double) v > BRIDGE_NUM_MAX_EXACT)
    {
        return 0;
    }
    *out = (double) v;
    return 1;
}

#endif

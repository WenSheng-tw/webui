/*
 * test_aid.c  --  C helpers for the W1 tests (C99, test only, not part of the bridge).
 *
 *   W1T_NUM( cKind )                 NaN / infinity / limit doubles that Harbour code cannot
 *                                    write as literals
 *   W1T_N2SIZE( n )                  bridge_n2size() accepts n?
 *   W1T_RANGE( cKind, n1, n2 )       rule 9 predicates of bridge_core.c
 *   W1T_COUNTER( cName )             allocation counters of the cnt build (0 otherwise)
 */

#include <stdio.h>
#include <string.h>

#include "bridge_int.h"
#include "bridge_num.h"

extern int abi_counters_enabled(void);
extern void abi_counters_get(unsigned long *allocs, unsigned long *frees, unsigned long *live_blocks,
                             unsigned long *live_bytes, unsigned long *foreign_frees, unsigned long *table_full);

void HB_FUN_W1T_NUM(void)
{
    volatile double zero = 0.0;
    const char *k = hb_parc(1);
    double d = 0.0;

    if (k == NULL)
    {
        hb_ret();
        return;
    }
    if (strcmp(k, "nan") == 0)
    {
        d = zero / zero;
    }
    else if (strcmp(k, "inf") == 0)
    {
        d = 1.0 / zero;
    }
    else if (strcmp(k, "-inf") == 0)
    {
        d = -1.0 / zero;
    }
    else if (strcmp(k, "2^53") == 0)
    {
        d = BRIDGE_NUM_MAX_EXACT;
    }
    else if (strcmp(k, "2^53+2") == 0)
    {
        d = BRIDGE_NUM_MAX_EXACT + 2.0;
    }
    else if (strcmp(k, "2^64") == 0)
    {
        d = 18446744073709551616.0;
    }
    hb_retndlen(d, 0, 0);
}

void HB_FUN_W1T_N2SIZE(void)
{
    size_t v;
    double d;
    double in = hb_parnd(1);

    hb_retl(bridge_n2size(in, &v) && bridge_size2n(v, &d) && d == in);
}

void HB_FUN_W1T_RANGE(void)
{
    const char *k = hb_parc(1);
    size_t a, b;
    int ok = 0;

    if (k == NULL || !bridge_n2size(hb_parnd(2), &a) || !bridge_n2size(hb_parnd(3), &b))
    {
        hb_retl(0);
        return;
    }
    if (strcmp(k, "size") == 0)
    {
        ok = bridge_range_window_size(a, b);
    }
    else if (strcmp(k, "pos") == 0)
    {
        ok = bridge_range_window_pos(a, b);
    }
    else if (strcmp(k, "script") == 0)
    {
        ok = bridge_range_script_timeout(a);
    }
    else if (strcmp(k, "timeout") == 0)
    {
        ok = bridge_range_ok(a, 0, BRIDGE_TIMEOUT_MAX);
    }
    else if (strcmp(k, "argidx") == 0)
    {
        ok = bridge_range_ok(a, 0, BRIDGE_ARG_INDEX_MAX);
    }
    hb_retl(ok);
}

void HB_FUN_W1T_COUNTER(void)
{
    const char *k = hb_parc(1);
    unsigned long allocs, frees, blocks, bytes, foreign, full;
    unsigned long v = 0;

    abi_counters_get(&allocs, &frees, &blocks, &bytes, &foreign, &full);
    if (k != NULL)
    {
        if (strcmp(k, "enabled") == 0)
        {
            v = (unsigned long) abi_counters_enabled();
        }
        else if (strcmp(k, "allocs") == 0)
        {
            v = allocs;
        }
        else if (strcmp(k, "frees") == 0)
        {
            v = frees;
        }
        else if (strcmp(k, "blocks") == 0)
        {
            v = blocks;
        }
        else if (strcmp(k, "bytes") == 0)
        {
            v = bytes;
        }
        else if (strcmp(k, "foreign_frees") == 0)
        {
            v = foreign;
        }
        else if (strcmp(k, "table_full") == 0)
        {
            v = full;
        }
    }
    hb_retndlen((double) v, 0, 0);
}

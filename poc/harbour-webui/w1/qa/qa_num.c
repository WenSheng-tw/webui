/*
 * qa_num.c  --  QA helper: doubles that Harbour source cannot write as literals.
 * Independent of the Back-end test aid (tests/test_aid.c). C99, test only.
 */

#include <string.h>

#include "bridge_hb.h"

void HB_FUN_QA_NUM(void)
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
    else if (strcmp(k, "-nan") == 0)
    {
        d = -(zero / zero);
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
        d = 9007199254740992.0;
    }
    else if (strcmp(k, "2^53+2") == 0)
    {
        d = 9007199254740994.0;
    }
    else if (strcmp(k, "2^63") == 0)
    {
        d = 9223372036854775808.0;
    }
    else if (strcmp(k, "2^64") == 0)
    {
        d = 18446744073709551616.0;
    }
    else if (strcmp(k, "-2^63") == 0)
    {
        d = -9223372036854775808.0;
    }
    else if (strcmp(k, "denorm") == 0)
    {
        d = 4.9406564584124654e-324;
    }
    else if (strcmp(k, "dmax") == 0)
    {
        d = 1.7976931348623157e308;
    }
    hb_retndlen(d, 0, 0);
}

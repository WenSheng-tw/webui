#!/bin/sh
# PoC-002 W1 / A7: every bridge C file builds under strict ISO C89, with no extra warning.
# bridge_i64.c (W2) will be the only C99 file; until then there is none.
cd "$(dirname "$0")/../src" || exit 2
CC=${CC:-gcc}
rc=0
for f in *.c; do
    case $f in
        bridge_i64.c) FLAGS="-std=c99 -pedantic-errors -Wall -Wextra" ;;
        *) FLAGS="-std=c89 -pedantic-errors -Wall -Wextra -Werror" ;;
    esac
    if $CC $FLAGS -O2 -c "$f" -o /dev/null 2> /tmp/check-c89.$$; then
        echo "ok - $f ($FLAGS)"
    else
        echo "not ok - $f"
        cat /tmp/check-c89.$$
        rc=1
    fi
done
for f in *.h; do
    if grep -n '//' "$f" | grep -v 'http' > /dev/null; then
        echo "not ok - $f contains a // comment"
        rc=1
    else
        echo "ok - $f has no // comment"
    fi
    if (cat "$f"; echo 'typedef int bridge_check_tu;') | $CC -std=c89 -pedantic-errors -Wall -Wextra -Werror -I. -x c -fsyntax-only - 2> /dev/null; then
        echo "ok - $f compiles on its own as C89"
    else
        echo "not ok - $f does not compile on its own as C89"
        rc=1
    fi
done
rm -f /tmp/check-c89.$$
exit $rc

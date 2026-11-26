#!/bin/sh
# PoC-002 W1: build the two test executables (spec section 8: dual-mode runner).
#   ./build.sh [variant]      variant: O2 (default), O0, asan, cnt, vg, tsan
# Output: build/<variant>/w1run-linked  (mode 1, tests linked in)
#         build/<variant>/w1run-hrb     (mode 2, tests loaded from tests/t_w1.hrb)
# The bridge sources are compiled here with strict C89 flags, then handed to hbmk2 as
# objects; hbmk2's own C flags (Harbour headers are not C89) never touch them.

cd "$(dirname "$0")" || exit 2
V=${1:-O2}
HERE=$(pwd)
REPO=$(cd ../../.. && pwd)
SRC=../src
OUT=build/$V
export PATH=/opt/harbour/bin:$PATH
CC=${CC:-gcc}

case $V in
    O0)   OPT="-O0 -g" ;;
    O2)   OPT="-O2 -g" ;;
    asan) OPT="-O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined -fno-sanitize-recover=all" ;;
    cnt)  OPT="-O2 -g" ;;
    tsan) OPT="-O1 -g -fno-omit-frame-pointer -fsanitize=thread" ;;
    vg)   OPT="-O0 -g" ;;
    *) echo "unknown variant $V"; exit 2 ;;
esac

mkdir -p "$OUT/obj" "$OUT/hbw-linked" "$OUT/hbw-hrb" || exit 2
C89="-std=c89 -pedantic-errors -Wall -Wextra -I$SRC"
for f in bridge_core bridge_win bridge_event bridge; do
    $CC $C89 $OPT -c $SRC/$f.c -o "$OUT/obj/$f.o" || exit 1
done
$CC -std=c99 -pedantic-errors -Wall -Wextra -I$SRC $OPT -c tests/test_aid.c -o "$OUT/obj/test_aid.o" || exit 1
EXTRA_OBJ=
HB_LDFLAGS=
if [ "$V" = cnt ]; then
    $CC -std=c99 -Wall -Wextra -DABI_COUNTERS $OPT -c ../w0/abi_counters.c -o "$OUT/obj/abi_counters.o" || exit 1
    EXTRA_OBJ="$OUT/obj/abi_counters.o"
    HB_LDFLAGS="-ldflag=-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=free"
else
    $CC -std=c99 -Wall -Wextra $OPT -c ../w0/abi_counters.c -o "$OUT/obj/abi_counters.o" || exit 1
    EXTRA_OBJ="$OUT/obj/abi_counters.o"
fi
if [ "$V" = asan ]; then
    HB_LDFLAGS="-ldflag=-fsanitize=address,undefined"
fi
if [ "$V" = tsan ]; then
    HB_LDFLAGS="-ldflag=-fsanitize=thread"
fi

OBJS="$OUT/obj/bridge_core.o $OUT/obj/bridge_win.o $OUT/obj/bridge_event.o $OUT/obj/bridge.o $OUT/obj/test_aid.o $EXTRA_OBJ"
LIBS="-L$REPO/dist -lwebui-2-static -lpthread -lm -ldl"
COMMON="runner.prg hrbtool.prg testlib.prg selftest.prg bridge_request.prg"

# shellcheck disable=SC2086
hbmk2 -q -hbexe -gtcgi -w3 -es2 -o$OUT/w1run-linked -DMODE_LINKED $COMMON tests/t_w1.prg $OBJS $LIBS $HB_LDFLAGS \
    -workdir=$OUT/hbw-linked || exit 1
# shellcheck disable=SC2086
hbmk2 -q -hbexe -gtcgi -w3 -es2 -o$OUT/w1run-hrb -DMODE_HRB $COMMON $OBJS $LIBS $HB_LDFLAGS \
    -workdir=$OUT/hbw-hrb || exit 1
echo "built $OUT/w1run-linked $OUT/w1run-hrb"

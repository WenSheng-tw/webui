#!/bin/sh
# QA negative-input tests for the W1 bridge (rule 9). Builds qa_neg against the real bridge
# sources (strict C89 flags as in ../build.sh) and runs every case in its own process under
# Xvfb. A case passes only if the exe exits 0 with no "not ok" line, so a crash (signal) or a
# missing TAP end is a failure.
#   ./run-qa-neg.sh [O2|asan] [mutant1|mutant2]
# The mutants patch a COPY of the sources under build/ (never the real ones) and must make
# the suite FAIL; this proves the tests can detect an accepted out-of-range value.

cd "$(dirname "$0")" || exit 2
export PATH=/opt/harbour/bin:$PATH
V=${1:-O2}
MUT=${2:-}
case $V in
    O2) OPT="-O2 -g"; LD="" ;;
    asan) OPT="-O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined -fno-sanitize-recover=all"; LD="-ldflag=-fsanitize=address,undefined" ;;
    *) echo "unknown variant $V"; exit 2 ;;
esac
REPO=$(cd ../../../.. && pwd)
SRC=$(pwd)/../../src
OUT=build/$V${MUT:+-$MUT}
mkdir -p "$OUT/obj" "$OUT/hbw" results
if [ -n "$MUT" ]; then
    rm -rf "$OUT/src"; mkdir -p "$OUT/src"; cp "$SRC"/*.c "$SRC"/*.h "$OUT/src/"
    SRC=$(pwd)/$OUT/src
    case $MUT in
        mutant1) sed -i 's/BRIDGE_TIMEOUT_MAX 60UL/BRIDGE_TIMEOUT_MAX 61UL/' "$SRC/bridge_int.h" ;;
        mutant2) sed -i 's/if (!bridge_range_ok(v, lo, hi))/if (0)/' "$SRC/bridge_core.c" ;;
        *) echo "unknown mutant"; exit 2 ;;
    esac
    if diff -rq ../../src "$SRC" > /dev/null; then echo "mutant patch did not change the sources"; exit 2; fi
fi
for f in bridge_core bridge_win bridge_event bridge; do
    gcc -std=c89 -pedantic-errors -Wall -Wextra -I"$SRC" $OPT -c "$SRC/$f.c" -o "$OUT/obj/$f.o" || exit 1
done
gcc -std=c99 -pedantic-errors -Wall -Wextra -I"$SRC" $OPT -c qa_num.c -o "$OUT/obj/qa_num.o" || exit 1
# shellcheck disable=SC2086
hbmk2 -q -hbexe -gtcgi -w3 -es2 -o"$OUT/qa_neg" qa_neg.prg ../testlib.prg \
    "$OUT/obj/bridge_core.o" "$OUT/obj/bridge_win.o" "$OUT/obj/bridge_event.o" "$OUT/obj/bridge.o" "$OUT/obj/qa_num.o" \
    -L"$REPO/dist" -lwebui-2-static -lpthread -lm -ldl $LD -workdir="$OUT/hbw" > "$OUT/build.log" 2>&1 || { echo "build failed, see $OUT/build.log"; exit 1; }

FAIL=0
TOTAL=0
for C in timeout config window bind show state table; do
    timeout 120 xvfb-run -a "$OUT/qa_neg" --case=$C > "$OUT/$C.tap" 2> "$OUT/$C.err"
    rc=$?
    n=$(grep -c '^ok' "$OUT/$C.tap"); f=$(grep -c '^not ok' "$OUT/$C.tap")
    TOTAL=$((TOTAL + n))
    if [ $rc = 0 ] && [ "$f" = 0 ] && grep -q "reached its end without a crash" "$OUT/$C.tap"; then
        echo "ok: case $C ($n ok, 0 not ok, exit 0)"
    else
        echo "FAIL: case $C exit $rc, $n ok, $f not ok (see $OUT/$C.tap, $OUT/$C.err)"; FAIL=1
    fi
done
if [ -z "$MUT" ]; then
    timeout 120 xvfb-run -a "$OUT/qa_neg" --case=finding_exit_first > "$OUT/finding_exit_first.tap" 2> /dev/null
    if grep -q '^not ok' "$OUT/finding_exit_first.tap"; then
        echo "FINDING F1 (open, not counted): new_window returns 0 after a lone exit + clean, see $OUT/finding_exit_first.tap"
    else
        echo "finding F1 no longer reproduces"
    fi
fi
echo "== $V${MUT:+ $MUT}: $TOTAL ok in total, $([ $FAIL = 0 ] && echo PASSED || echo FAILED)"
exit $FAIL

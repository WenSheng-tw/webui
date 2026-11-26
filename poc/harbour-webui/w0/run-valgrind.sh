#!/bin/sh
# PoC-002 W0 / spec A8: valgrind on the headless ABI test (variant vg: -O0 -g, no sanitizer).
#   ./run-valgrind.sh               full headless group (1000 iterations per C1 case)
#   ITER=20 ./run-valgrind.sh       fewer C1 iterations (quicker)
# Other targets (W1 and later): VG_TARGET=<exe> VG_ARGS="<args>" VG_PREFIX="xvfb-run -a" VG_OUT=<dir>
# VG_TAG=<name> ./run-valgrind.sh runs that executable instead of the W0 ABI test (no build step).
# Pass 1 (raw, no suppressions) classifies every definite leak: GTK/glib constructors of
# libraries that WebUI dlopen()s versus bridge/WebUI. Pass 2 applies valgrind-dlopen.supp and
# must end with zero definite leaks (--errors-for-leak-kinds=definite, --error-exitcode=99).

cd "$(dirname "$0")" || exit 2
HERE=$(pwd)
ITER=${ITER:-1000}
command -v valgrind > /dev/null 2>&1 || { echo "valgrind not installed"; exit 2; }
if [ -n "$VG_TARGET" ]; then
    case $VG_TARGET in /*) ;; *) VG_TARGET=$(cd "$(dirname "$VG_TARGET")" && pwd)/$(basename "$VG_TARGET") ;; esac
    OUT=${VG_OUT:-build}
    TAG=${VG_TAG:-target}
    BIN=$VG_TARGET
    ARGS=$VG_ARGS
    mkdir -p "$OUT"
    PREFIX_LABEL=$TAG
else
    OUT=build
    TAG=w0
    mkdir -p build
    make build-vg > build/build-vg.log 2>&1 || { echo "build-vg failed, see build/build-vg.log"; exit 2; }
    BIN=build/vg/abi_test
    ARGS="--headless --iterations $ITER"
fi

echo "== valgrind $(valgrind --version), target $BIN $ARGS"
echo "== pass 1: raw, no suppressions"
# shellcheck disable=SC2086
$VG_PREFIX valgrind --leak-check=full --show-leak-kinds=definite,indirect,possible --errors-for-leak-kinds=definite \
    --num-callers=30 --log-file="$OUT/vg-raw.log" $BIN $ARGS \
    > "$OUT/vg-raw.tap" 2> "$OUT/vg-raw.err"
echo "test exit code: $?  ($(grep -c '^ok ' "$OUT/vg-raw.tap") ok, $(grep -c '^not ok ' "$OUT/vg-raw.tap") not ok)"
grep -E "total heap usage|definitely lost|indirectly lost|possibly lost|still reachable|ERROR SUMMARY" "$OUT/vg-raw.log" | sed 's/^==[0-9]*== /  /'
python3 "$HERE/vg_classify.py" "$OUT/vg-raw.log" | tee "$OUT/vg-classify.txt" | grep '^CLASS'

echo "== pass 2: with valgrind-dlopen.supp, definite leaks are errors"
# shellcheck disable=SC2086
$VG_PREFIX valgrind --leak-check=full --show-leak-kinds=definite,indirect,possible --errors-for-leak-kinds=definite \
    --error-exitcode=99 --suppressions="$HERE/valgrind-dlopen.supp" -s --num-callers=30 \
    --log-file="$OUT/vg-supp.log" $BIN $ARGS \
    > "$OUT/vg-supp.tap" 2> "$OUT/vg-supp.err"
RC=$?
echo "valgrind exit code: $RC  ($(grep -c '^ok ' "$OUT/vg-supp.tap") ok, $(grep -c '^not ok ' "$OUT/vg-supp.tap") not ok)"
grep -E "total heap usage|definitely lost|indirectly lost|possibly lost|still reachable|suppressed:|ERROR SUMMARY" "$OUT/vg-supp.log" | sed 's/^==[0-9]*== /  /'
grep -E "^ *[0-9]+ .*dlopen-gtk" "$OUT/vg-supp.log" | sed 's/^==[0-9]*== /  used suppression: /'
exit "$RC"

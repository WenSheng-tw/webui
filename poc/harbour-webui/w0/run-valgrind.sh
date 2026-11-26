#!/bin/sh
# PoC-002 W0 / spec A8: valgrind on the headless ABI test (variant vg: -O0 -g, no sanitizer).
#   ./run-valgrind.sh               full headless group (1000 iterations per C1 case)
#   ITER=20 ./run-valgrind.sh       fewer C1 iterations (quicker)
# Pass 1 (raw, no suppressions) classifies every definite leak: GTK/glib constructors of
# libraries that WebUI dlopen()s versus bridge/WebUI. Pass 2 applies valgrind-dlopen.supp and
# must end with zero definite leaks (--errors-for-leak-kinds=definite, --error-exitcode=99).

cd "$(dirname "$0")" || exit 2
ITER=${ITER:-1000}
mkdir -p build
command -v valgrind > /dev/null 2>&1 || { echo "valgrind not installed"; exit 2; }
make build-vg > build/build-vg.log 2>&1 || { echo "build-vg failed, see build/build-vg.log"; exit 2; }

echo "== valgrind $(valgrind --version), C1 iterations $ITER"
echo "== pass 1: raw, no suppressions"
valgrind --leak-check=full --show-leak-kinds=definite,indirect,possible --errors-for-leak-kinds=definite \
    --num-callers=30 --log-file=build/vg-raw.log build/vg/abi_test --headless --iterations "$ITER" \
    > build/vg-raw.tap 2> build/vg-raw.err
echo "test exit code: $?  ($(grep -c '^ok ' build/vg-raw.tap) ok, $(grep -c '^not ok ' build/vg-raw.tap) not ok)"
grep -E "total heap usage|definitely lost|indirectly lost|possibly lost|still reachable|ERROR SUMMARY" build/vg-raw.log | sed 's/^==[0-9]*== /  /'
python3 vg_classify.py build/vg-raw.log | tee build/vg-classify.txt | grep '^CLASS'

echo "== pass 2: with valgrind-dlopen.supp, definite leaks are errors"
valgrind --leak-check=full --show-leak-kinds=definite,indirect,possible --errors-for-leak-kinds=definite \
    --error-exitcode=99 --suppressions=valgrind-dlopen.supp -s --num-callers=30 \
    --log-file=build/vg-supp.log build/vg/abi_test --headless --iterations "$ITER" \
    > build/vg-supp.tap 2> build/vg-supp.err
RC=$?
echo "valgrind exit code: $RC  ($(grep -c '^ok ' build/vg-supp.tap) ok, $(grep -c '^not ok ' build/vg-supp.tap) not ok)"
grep -E "total heap usage|definitely lost|indirectly lost|possibly lost|still reachable|suppressed:|ERROR SUMMARY" build/vg-supp.log | sed 's/^==[0-9]*== /  /'
grep -E "^ *[0-9]+ .*dlopen-gtk" build/vg-supp.log | sed 's/^==[0-9]*== /  used suppression: /'
exit "$RC"

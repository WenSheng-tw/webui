#!/bin/sh
# PoC-002 W1 / A8: valgrind on the real bridge. Runs the W0 runner (../w0/run-valgrind.sh,
# raw pass + suppression pass) on the vg build of both test executables with the full suite
# (headless + Chrome under Xvfb). Bridge and WebUI definite leaks must be 0; the GTK/glib
# constructor leaks of the dlopen-ed libraries are classified and suppressed separately.
#   ./run-valgrind.sh               both modes
# Output: build/results/vg-<mode>/*.log, summary on stdout.

cd "$(dirname "$0")" || exit 2
./build.sh vg > build/results/build-vg.log 2>&1 || { mkdir -p build/results; ./build.sh vg > build/results/build-vg.log 2>&1 || { echo "build vg failed"; exit 2; }; }
rc=0
for M in linked hrb; do
    echo "######## mode $M"
    rm -f tests/t_w1.hrb tests/t_w1.hrb.md5
    VG_TARGET=$(pwd)/build/vg/w1run-$M VG_ARGS="--dir=$(pwd)/tests" VG_PREFIX="timeout 600 xvfb-run -a" VG_OUT=$(pwd)/build/results/vg-$M VG_TAG=w1-$M \
        sh ../w0/run-valgrind.sh | tee build/results/vg-$M.txt
    grep -q "^CLASS bridge-webui definite_records=0 definite_direct_bytes=0" build/results/vg-$M.txt || { echo "FAIL: bridge/WebUI definite leaks in mode $M"; rc=1; }
    grep -q "valgrind exit code: 0 " build/results/vg-$M.txt || { echo "FAIL: suppressed pass of mode $M did not end with 0 errors"; rc=1; }
done
rm -f tests/t_w1.hrb tests/t_w1.hrb.md5
exit $rc

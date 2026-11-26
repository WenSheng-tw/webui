#!/bin/sh
# PoC-002 W0: run the whole ABI test matrix and summarise the TAP output.
#   ./run-abi.sh            static checks + {O0, O2, asan} x {headless, live}
#   VARIANTS="O2" ./run-abi.sh
#   NO_LIVE=1 ./run-abi.sh  skip group E
# Exit code 0 only if no "not ok" line appeared anywhere and nothing crashed.

cd "$(dirname "$0")" || exit 2
HERE=$(pwd)
REPO=$(cd ../../.. && pwd)
VARIANTS=${VARIANTS:-"O0 O2 asan cnt"}
mkdir -p build
FAIL=0

if [ ! -f "$REPO/dist/libwebui-2-static.a" ]; then
    echo "# dist/libwebui-2-static.a missing, running make at the repo root"
    make -C "$REPO" > build/lib-build.log 2>&1 || { echo "library build failed, see build/lib-build.log"; exit 2; }
fi

make abi_enums_gen.h > /dev/null || exit 2

run() {
    name=$1
    shift
    "$@" > "build/$name.tap" 2> "build/$name.err"
    rc=$?
    ok=$(grep -c '^ok ' "build/$name.tap")
    info=$(grep -c '^ok .*# informational' "build/$name.tap")
    bad=$(grep -c '^not ok ' "build/$name.tap")
    skip=$(grep -c '# SKIP' "build/$name.tap")
    printf '%-18s rc=%-3s ok=%-4s (informational %s) not_ok=%-3s skip=%s\n' "$name" "$rc" "$ok" "$info" "$bad" "$skip"
    if [ "$rc" -ne 0 ] || [ "$bad" -ne 0 ]; then
        FAIL=1
        grep '^not ok' "build/$name.tap" | sed 's/^/    /'
        [ -s "build/$name.err" ] && head -n 20 "build/$name.err" | sed 's/^/    stderr: /'
    fi
}

echo "== static checks"
run functions python3 gen_functions.py
run protos python3 check_protos.py
run bool-objdump python3 check_bool_abi.py

LIVE_TOOL=""
if [ -z "$NO_LIVE" ]; then
    if command -v xvfb-run > /dev/null 2>&1 && { command -v google-chrome > /dev/null 2>&1 || command -v chromium > /dev/null 2>&1 || command -v chromium-browser > /dev/null 2>&1; }; then
        LIVE_TOOL="xvfb-run -a -s '-screen 0 1280x1024x24'"
    else
        echo "# group E skipped: xvfb-run or a Chrome/Chromium binary is missing"
    fi
fi

for v in $VARIANTS; do
    echo "== variant $v"
    make "build-$v" > "build/build-$v.log" 2>&1 || { echo "build-$v FAILED, see build/build-$v.log"; FAIL=1; continue; }
    run "$v-headless" "build/$v/abi_test" --headless
    if [ -n "$LIVE_TOOL" ]; then
        run "$v-live" timeout 300 xvfb-run -a -s "-screen 0 1280x1024x24" "build/$v/abi_test" --live --dir "$HERE"
    fi
done

if [ "$FAIL" -eq 0 ]; then
    echo "RESULT: all runs passed"
else
    echo "RESULT: FAILURES, see build/*.tap"
fi
exit "$FAIL"

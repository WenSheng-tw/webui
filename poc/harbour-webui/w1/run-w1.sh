#!/bin/sh
# PoC-002 W1 test matrix.
#   ./run-w1.sh [variant ...]     default variants: O0 O2 asan cnt
# For each variant: build, executor self-test, mode 1 (linked) and mode 2 (hrb) with the full
# suite (headless + Chrome under Xvfb), then compare the TAP bodies of both modes.
# Variant independent: C89 check, W0 static checks, TLS exposure, hash rules on tests/t_w1.prg.
# Needs xvfb-run and Chrome for the GUI part. Results: build/results/<variant>-<mode>.tap
# and build/results/summary.txt; exit code 0 only if everything passed.

cd "$(dirname "$0")" || exit 2
export PATH=/opt/harbour/bin:$PATH
VARIANTS=${*:-O0 O2 asan cnt}
OUTDIR=build/results
mkdir -p "$OUTDIR"
SUM=$OUTDIR/summary.txt
: > "$SUM"
FAIL=0
say() { echo "$*" | tee -a "$SUM"; }
bad() { say "FAIL: $*"; FAIL=1; }
WD=${W1_TIMEOUT:-420}

say "== W1 run $(date -u +%Y-%m-%dT%H:%M:%SZ), $(gcc --version | head -1), $(hbmk2 -version 2>&1 | head -1)"

say "== static: C89 compile of every bridge unit (A7)"
./check-c89.sh > "$OUTDIR/c89.txt" 2>&1 && say "ok: check-c89.sh ($(grep -c '^ok' "$OUTDIR/c89.txt") checks)" || bad "check-c89.sh, see $OUTDIR/c89.txt"

say "== static: W0 checks (function list, enums, prototypes of bridge_decl.h)"
( cd ../w0 && python3 gen_functions.py > "../w1/$OUTDIR/w0-functions.tap" && python3 check_protos.py > "../w1/$OUTDIR/w0-protos.tap" ) \
    && say "ok: gen_functions.py and check_protos.py ($(grep -c '^ok' "$OUTDIR/w0-functions.tap") + $(grep -c '^ok' "$OUTDIR/w0-protos.tap") checks)" \
    || bad "W0 static checks, see $OUTDIR/w0-*.tap"
( cd ../w0 && python3 gen_functions.py --request /tmp/w1-full-request.prg > /dev/null && diff -q /tmp/w1-full-request.prg bridge_request.prg > /dev/null \
    && python3 gen_functions.py --request /tmp/w1-w1-request.prg --upto W1 > /dev/null && diff -q /tmp/w1-w1-request.prg ../w1/bridge_request.prg > /dev/null ) \
    && say "ok: both generated REQUEST files are current" || bad "REQUEST files differ from the generator output"

say "== rule 7: the bridge does not expose or reference webui_set_tls_certificate"
./build.sh O2 > "$OUTDIR/build-O2.log" 2>&1 || { bad "build O2, see $OUTDIR/build-O2.log"; exit 1; }
if nm -u build/O2/obj/bridge_core.o build/O2/obj/bridge_win.o build/O2/obj/bridge_event.o build/O2/obj/bridge.o | grep -i tls > /dev/null; then
    bad "a bridge object references a TLS function"
elif nm build/O2/w1run-hrb | grep -i "HB_FUN_.*TLS" > /dev/null; then
    bad "the exe defines a Harbour TLS function"
else
    say "ok: no TLS symbol in the bridge objects or in the Harbour symbol table of the exe"
fi

say "== A13 hash rules on tests/t_w1.prg (copy in a temp folder, headless suite)"
T=$(mktemp -d)
cp tests/t_w1.prg tests/w1_page.html "$T/"
R=build/O2/w1run-hrb
act() { grep '^# hrb action:' "$1" | sed 's/.*action: //'; }
$R --dir="$T" --suite=headless > "$T/1.tap" 2>&1; a1=$(act "$T/1.tap")
[ "$a1" = compile ] && [ -f "$T/t_w1.hrb" ] && [ -f "$T/t_w1.hrb.md5" ] && say "ok: no hrb, the runner compiles and writes hrb + md5 sidecar" || bad "A13 step 1 action=$a1"
sum1=$(md5sum "$T/t_w1.hrb" | cut -d' ' -f1)
$R --dir="$T" --suite=headless > "$T/2.tap" 2>&1; a2=$(act "$T/2.tap")
sum2=$(md5sum "$T/t_w1.hrb" | cut -d' ' -f1)
[ "$a2" = reuse ] && [ "$sum1" = "$sum2" ] && say "ok: identical hash skips the compile (hrb byte-identical)" || bad "A13 step 2 action=$a2"
printf '\n/* edited */\n' >> "$T/t_w1.prg"
$R --dir="$T" --suite=headless > "$T/3.tap" 2>&1; a3=$(act "$T/3.tap")
side=$(head -1 "$T/t_w1.hrb.md5")
[ "$a3" = compile ] && [ "$side" = "$(md5sum "$T/t_w1.prg" | cut -d' ' -f1)" ] && say "ok: changed prg recompiles and the sidecar holds the new md5" || bad "A13 step 3 action=$a3 sidecar=$side"
$R --dir="$T" --suite=headless > "$T/3b.tap" 2>&1; [ "$(act "$T/3b.tap")" = reuse ] && say "ok: unchanged again reuses" || bad "A13 step 3b"
rm -f "$T/t_w1.prg"
$R --dir="$T" --suite=headless > "$T/4.tap" 2>&1; a4=$(act "$T/4.tap"); rc4=$?
[ "$a4" = hrb-only ] && ! grep -q '^not ok' "$T/4.tap" && say "ok: prg missing, the existing hrb is used and its tests pass" || bad "A13 step 4 action=$a4"
rm -f "$T/t_w1.hrb"
$R --dir="$T" --suite=headless > "$T/5.tap" 2>&1; a5=$(act "$T/5.tap")
[ "$a5" = missing ] && say "ok: prg and hrb both missing reports missing (test fails by design)" || bad "A13 step 5 action=$a5"
mkdir -p "$OUTDIR/a13" && cp "$T"/*.tap "$OUTDIR/a13/"
rm -rf "$T"
rm -f tests/t_w1.hrb tests/t_w1.hrb.md5

for V in $VARIANTS; do
    say "== variant $V"
    ./build.sh "$V" > "$OUTDIR/build-$V.log" 2>&1 || { bad "build $V, see $OUTDIR/build-$V.log"; continue; }
    B=build/$V
    timeout $WD "$B/w1run-linked" --selftest > "$OUTDIR/$V-selftest.tap" 2>&1
    n=$(grep -c '^ok' "$OUTDIR/$V-selftest.tap"); f=$(grep -c '^not ok' "$OUTDIR/$V-selftest.tap")
    [ "$f" = 0 ] && [ "$n" -gt 0 ] && say "ok: $V executor self-test ($n ok)" || bad "$V self-test: $f not ok"
    for M in linked hrb; do
        rm -f tests/t_w1.hrb tests/t_w1.hrb.md5
        timeout $WD xvfb-run -a "$B/w1run-$M" > "$OUTDIR/$V-$M.tap" 2> "$OUTDIR/$V-$M.err"
        rc=$?
        n=$(grep -c '^ok' "$OUTDIR/$V-$M.tap"); f=$(grep -c '^not ok' "$OUTDIR/$V-$M.tap")
        if [ $rc = 0 ] && [ "$f" = 0 ] && [ "$n" -gt 0 ]; then say "ok: $V mode $M ($n ok, 0 not ok)"; else bad "$V mode $M exit $rc, $n ok, $f not ok"; fi
    done
    grep -v '^#' "$OUTDIR/$V-linked.tap" > "$OUTDIR/$V-linked.body"
    grep -v '^#' "$OUTDIR/$V-hrb.tap" > "$OUTDIR/$V-hrb.body"
    sed -i 's/^TAP version.*//' "$OUTDIR/$V-linked.body" "$OUTDIR/$V-hrb.body"
    if diff "$OUTDIR/$V-linked.body" "$OUTDIR/$V-hrb.body" > "$OUTDIR/$V-diff.txt"; then
        say "ok: $V mode 1 and mode 2 produce identical TAP bodies (A14)"
    else
        bad "$V modes differ, see $OUTDIR/$V-diff.txt"
    fi
    rm -f tests/t_w1.hrb tests/t_w1.hrb.md5
done

[ "$FAIL" = 0 ] && say "== ALL PASSED" || say "== FAILURES, see above"
exit $FAIL

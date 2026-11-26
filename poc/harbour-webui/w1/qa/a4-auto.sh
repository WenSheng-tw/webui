#!/bin/sh
# Automatic check of PoC-001 A4 (three clicks give call counts 1, 2, 3 in order) against the
# W1 bridge. poc001-check.sh only saves a screenshot for a human; this script reads the text
# the browser actually renders: after each click it selects the whole page (ctrl+a, ctrl+c)
# and reads the clipboard with xclip, then asserts the result line of THAT click.
#   ./a4-auto.sh [clicks]        default 3. With 2 the final assertion "call #3" must FAIL
#                                (negative control, used by the QA to prove the check can fail).
# Own Xvfb display :89 (poc001-check.sh uses :88). Exit code 0 only if every click shows the
# expected call number. Output: build/a4/
cd "$(dirname "$0")/../.." || exit 2
export PATH=/opt/harbour/bin:$PATH
CLICKS=${1:-3}
OUT=w1/qa/build/a4
mkdir -p "$OUT" /tmp/hbw-qa-a4
FAIL=0
ok() { echo "ok - $*"; }
nok() { echo "not ok - $*"; FAIL=1; }

hbmk2 -q prg/main.prg prg/hello.prg src/bridge_core.c src/bridge_win.c src/bridge_event.c src/bridge.c \
    -I../../include -L../../dist -lwebui-2-static -lpthread -lm -ldl -o"$OUT/hbwebui" -workdir=/tmp/hbw-qa-a4 > "$OUT/build.log" 2>&1 \
    || { echo "not ok - build, see $OUT/build.log"; exit 1; }

Xvfb :89 -screen 0 1280x800x24 > /dev/null 2>&1 &
XV=$!
sleep 1
export DISPLAY=:89
"$OUT/hbwebui" > "$OUT/run.out" 2>&1 &
APP=$!
sleep 4
if ! kill -0 $APP 2> /dev/null; then nok "program exited before the clicks"; kill $XV; exit 1; fi

i=1
while [ $i -le $CLICKS ]; do
    xdotool mousemove 382 155 click 1
    sleep 1
    xclip -selection clipboard -d :89 < /dev/null 2> /dev/null
    xdotool key ctrl+a
    xdotool key ctrl+c
    sleep 0.5
    xclip -selection clipboard -o -d :89 > "$OUT/page-$i.txt" 2> /dev/null
    if grep -q "Harbour says: Hello, World! (call #$i)" "$OUT/page-$i.txt"; then
        ok "A4 click $i shows 'Harbour says: Hello, World! (call #$i)'"
    else
        nok "A4 click $i: expected call #$i, page text is: $(tr '\n' ' ' < "$OUT/page-$i.txt")"
    fi
    i=$((i + 1))
done
if [ "$CLICKS" -lt 3 ]; then
    if grep -q "(call #3)" "$OUT/page-$CLICKS.txt"; then ok "A4 call #3 seen"; else nok "A4 call #3 never shown (only $CLICKS clicks)"; fi
fi

xdotool mousemove 600 400 click 1
xdotool key ctrl+w
n=0
while [ $n -lt 10 ] && kill -0 $APP 2> /dev/null; do sleep 1; n=$((n + 1)); done
if kill -0 $APP 2> /dev/null; then kill $APP; nok "program still running after the window was closed"; else wait $APP; [ $? = 0 ] && ok "program ended with exit code 0 after the window closed" || nok "program exit code not 0"; fi
pkill -f "$OUT/hbwebui" 2> /dev/null
pkill chrome 2> /dev/null
kill $XV 2> /dev/null
exit $FAIL

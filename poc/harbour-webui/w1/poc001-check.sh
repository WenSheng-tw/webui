#!/bin/sh
# PoC-001 regression (A1 to A5) against the W1 bridge. Linux, Xvfb on :88, Chrome via WebUI.
#   A1 every bridge C file passes strict C89 (check-c89.sh)
#   A2 prg/main.prg + prg/hello.prg + the bridge files build and link with hbmk2
#   A3 the window opens (screenshot saved)
#   A4 three clicks on "Say Hello" give "call #3" (screenshot saved; read it by eye, there
#      is no OCR here)
#   A5 closing the window ends the program with exit code 0 within 10 seconds
# The click position (382,155) matches the layout of ui/index.html on a 1280x800 screen.
cd "$(dirname "$0")/.." || exit 2
export PATH=/opt/harbour/bin:$PATH
OUT=w1/build/results/poc001
mkdir -p "$OUT" /tmp/hbw-poc001
FAIL=0
ok() { echo "ok - $*"; }
nok() { echo "not ok - $*"; FAIL=1; }

if w1/check-c89.sh > "$OUT/c89.txt" 2>&1; then ok "A1 bridge files pass -std=c89 -pedantic-errors -Wall -Wextra"; else nok "A1 see $OUT/c89.txt"; fi

if hbmk2 -q prg/main.prg prg/hello.prg src/bridge_core.c src/bridge_win.c src/bridge_event.c src/bridge.c \
    -I../../include -L../../dist -lwebui-2-static -lpthread -lm -ldl -o"$OUT/hbwebui" -workdir=/tmp/hbw-poc001 > "$OUT/build.log" 2>&1; then
    ok "A2 build and link"
else
    nok "A2 build, see $OUT/build.log"
    exit 1
fi

Xvfb :88 -screen 0 1280x800x24 > /dev/null 2>&1 &
XV=$!
sleep 1
export DISPLAY=:88
"$OUT/hbwebui" > "$OUT/run.out" 2>&1 &
APP=$!
sleep 4
ffmpeg -loglevel error -f x11grab -video_size 1280x800 -i :88 -frames:v 1 -y "$OUT/a3-open.png"
if kill -0 $APP 2> /dev/null; then ok "A3 program is running with its window (see a3-open.png)"; else nok "A3 program exited early"; fi
i=0
while [ $i -lt 3 ]; do
    xdotool mousemove 382 155 click 1
    sleep 0.7
    i=$((i + 1))
done
sleep 0.5
ffmpeg -loglevel error -f x11grab -video_size 1280x800 -i :88 -frames:v 1 -y "$OUT/a4-call3.png"
echo "# A4: check $OUT/a4-call3.png shows 'Harbour says: Hello, World! (call #3)'"
xdotool mousemove 600 400 click 1
xdotool key ctrl+w
n=0
while [ $n -lt 10 ] && kill -0 $APP 2> /dev/null; do
    sleep 1
    n=$((n + 1))
done
if kill -0 $APP 2> /dev/null; then
    nok "A5 program still running after closing the window"
    kill $APP
else
    wait $APP
    rc=$?
    [ "$rc" = 0 ] && ok "A5 program ended with exit code 0 about $n s after closing the window" || nok "A5 exit code $rc"
fi
pkill chrome 2> /dev/null
kill $XV 2> /dev/null
exit $FAIL

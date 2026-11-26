#!/bin/sh
# Runs a show+wait program under Xvfb; kills the browser after 8 s so WEBUI_WAIT() returns
# (wait blocks while a window is connected, with or without a prior exit; see f1-ctl-show).
# usage: run-wait-case.sh <exe>
xvfb-run -a sh -c '"$1" & P=$!; sleep 8; pkill -f "[c]hrome"; wait $P; echo "exit code $?"' sh "$1"

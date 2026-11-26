#!/usr/bin/env python3
"""Classify the leak records of a valgrind log (run-valgrind.sh helper).

A record whose stack contains _dl_init below dlopen is a library constructor of a
dlopen-ed GTK/glib/WebKit library ("gtk-glib"); everything else is "bridge-webui"
(the test objects and libwebui-2-static.a). Prints one summary line per class.
"""

import re
import sys


def main(path):
    text = open(path, errors="replace").read()
    blocks = re.split(r"\n==\d+== \n", text)
    cls = {"gtk-glib": {}, "bridge-webui": {}}
    for blk in blocks:
        m = re.search(r"([\d,]+)(?: \(([\d,]+) direct, ([\d,]+) indirect\))? bytes in ([\d,]+) blocks? are (definitely|indirectly|possibly) lost", blk)
        if not m:
            continue
        kind = m.group(5)
        direct = int((m.group(2) or m.group(1)).replace(",", "")) if kind != "indirectly" else 0
        blocks_n = int(m.group(4).replace(",", ""))
        who = "gtk-glib" if "_dl_init" in blk else "bridge-webui"
        d = cls[who].setdefault(kind, [0, 0])
        d[0] += blocks_n if kind != "indirectly" else 0
        d[1] += direct
        if who == "bridge-webui":
            sys.stdout.write("# bridge-webui record:\n" + blk + "\n")
    for who in ("bridge-webui", "gtk-glib"):
        dm = cls[who].get("definitely", [0, 0])
        pm = cls[who].get("possibly", [0, 0])
        sys.stdout.write("CLASS %s definite_records=%d definite_direct_bytes=%d possible_records=%d\n" % (who, dm[0], dm[1], pm[0]))


if __name__ == "__main__":
    main(sys.argv[1])

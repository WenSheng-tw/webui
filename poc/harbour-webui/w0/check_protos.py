#!/usr/bin/env python3
"""W0 group A (static part): enum drift check and prototype diff, TAP output.

  check_protos.py              run the checks (exit 1 on any failure)
  check_protos.py --gen-enums  also (re)write abi_enums_gen.h from include/webui.h

A3: every enumerator of include/webui.h has a BRIDGE_* macro in bridge_consts.h with the
    same value, and bridge_consts.h defines nothing else of that family.
A4: all 112 WEBUI_EXPORT prototypes of include/webui.h are normalized (bool result ->
    ubool, bool and enum parameters -> int, parameter names dropped) and compared with the
    extern declarations of bridge_decl.h and abi_i64.c. In-scope functions must match
    exactly, excluded functions must not be declared, nothing else may be declared.
"""

import os
import re
import sys

import hdrparse

HERE = hdrparse.HERE
SRC = os.path.join(HERE, "..", "src")

ENUM_PREFIX = {
    "webui_config": ("BRIDGE_CFG_", ""),
    "webui_browser": ("BRIDGE_BROWSER_", ""),
    "webui_runtime": ("BRIDGE_RUNTIME_", ""),
    "webui_event": ("BRIDGE_EVENT_", "WEBUI_EVENT_"),
    "webui_logger_level": ("BRIDGE_LOGGER_", "WEBUI_LOGGER_LEVEL_"),
}


def const_name(tag, enumerator):
    prefix, strip = ENUM_PREFIX[tag]
    if strip and enumerator.startswith(strip):
        enumerator = enumerator[len(strip):]
    return prefix + enumerator.upper()


def read_consts(path):
    with open(path) as f:
        text = hdrparse.strip_comments(f.read())
    return {m.group(1): int(m.group(2), 0) for m in re.finditer(r"(?m)^\s*#define\s+(BRIDGE_\w+)\s+(\d+)\s*$", text)}


def diff_protos(header, decls, excluded):
    """Return (diffs, missing, extra, wrongly_declared) for the in-scope comparison."""
    diffs, missing, wrongly = [], [], []
    for (name, ret, args), _raw in header:
        want = (name, ret, args)
        if name in excluded:
            if name in decls:
                wrongly.append(name)
            continue
        if name not in decls:
            missing.append(name)
        elif decls[name] != want:
            diffs.append("%s: header %s %s(%s) / bridge %s %s(%s)" % (
                name, ret, name, ", ".join(args), decls[name][1], name, ", ".join(decls[name][2])))
    known = set(p[0][0] for p in header)
    extra = sorted(set(decls) - known)
    return diffs, missing, extra, wrongly


def main():
    out, state = [], {"n": 0, "bad": 0}

    def ok(cond, desc, diag=()):
        state["n"] += 1
        if not cond:
            state["bad"] += 1
        out.append("%s %d - %s" % ("ok" if cond else "not ok", state["n"], desc))
        if not cond:
            out.extend("# " + d for d in diag)

    # A3: enum drift
    enums = hdrparse.header_enums()
    consts = read_consts(os.path.join(SRC, "bridge_consts.h"))
    expected = {}
    for tag in ENUM_PREFIX:
        for name, val in enums[tag]:
            expected[const_name(tag, name)] = val
    ok(sorted(enums) == sorted(ENUM_PREFIX), "A3 header enums are exactly %s" % ", ".join(sorted(ENUM_PREFIX)),
       ["found: %s" % sorted(enums)])
    total = len(expected)
    wrong = ["%s: header %s / bridge_consts.h %s" % (k, v, consts.get(k)) for k, v in sorted(expected.items())
             if k in consts and consts[k] != v]
    absent = sorted(set(expected) - set(consts))
    extra_c = sorted(set(consts) - set(expected))
    ok(not wrong and not absent and not extra_c,
       "A3 all %d enumerators match bridge_consts.h (wrong %d, missing %d, extra %d)" % (total, len(wrong), len(absent), len(extra_c)),
       wrong + ["missing: " + a for a in absent] + ["extra: " + e for e in extra_c])

    # A4: prototype diff
    header = hdrparse.header_protos()
    protos = [p for p, _ in header]
    rows = hdrparse.read_functions()
    excluded = set(r[0] for r in rows if r[1] == "EXCLUDED")
    decls = hdrparse.decl_protos(os.path.join(SRC, "bridge_decl.h"))
    decls_i64 = hdrparse.decl_protos(os.path.join(HERE, "abi_i64.c"))
    overlap = sorted(set(decls) & set(decls_i64))
    ok(not overlap, "A4 no function declared in both bridge_decl.h and abi_i64.c", overlap)
    ok(sorted(decls_i64) == sorted(["webui_get_int_at", "webui_get_int", "webui_return_int", "webui_interface_get_int_at"]),
       "A4 abi_i64.c declares exactly the four long long functions", sorted(decls_i64))
    every = dict(decls)
    every.update(decls_i64)

    ok(len(protos) == 112, "A4 header has 112 prototypes (got %d)" % len(protos))
    diffs, missing, extra, wrongly = diff_protos(header, every, excluded)
    compared = len(protos) - len(excluded)
    ok(not diffs, "A4 %d in-scope prototypes: zero signature diffs (got %d)" % (compared, len(diffs)), diffs)
    ok(not missing, "A4 zero in-scope prototypes missing from the bridge (got %d)" % len(missing), missing)
    ok(not extra, "A4 zero extra declarations not in the header (got %d)" % len(extra), extra)
    ok(not wrongly, "A4 none of the %d excluded functions is declared (got %d)" % (len(excluded), len(wrongly)), wrongly)
    ok(compared + len(excluded) == 112 and len(every) == compared,
       "A4 112 = %d compared + %d excluded, bridge declares %d" % (compared, len(excluded), len(every)))
    long_long = sorted(n for (n, r, a), _ in header if "long long" in r or "long long" in " ".join(a))
    ok(sorted(decls_i64) == long_long, "A4 every header prototype using long long is in abi_i64.c only", long_long)

    # Negative controls: the diff must notice damage.
    damaged = dict(every)
    n, r, a = damaged["webui_set_public"]
    damaged["webui_set_public"] = (n, r, [a[0], "ubool"])
    d2 = diff_protos(header, damaged, excluded)[0]
    ok(len(d2) == 1, "A4 control: a bool parameter declared ubool instead of int is reported")
    damaged = dict(every)
    del damaged["webui_show"]
    damaged["webui_malloc"] = ("webui_malloc", "void*", ["size_t"])
    d3 = diff_protos(header, damaged, excluded)
    ok(d3[1] == ["webui_show"] and d3[3] == ["webui_malloc"], "A4 control: a missing and a forbidden declaration are reported")
    damaged = dict(every)
    n, r, a = damaged["webui_set_size"]
    damaged["webui_set_size"] = (n, r, [a[0], "size_t", a[2]])
    ok(len(diff_protos(header, damaged, excluded)[0]) == 1, "A4 control: a widened unsigned int parameter is reported")

    if "--gen-enums" in sys.argv:
        lines = ["/* GENERATED by check_protos.py --gen-enums from include/webui.h. Do not edit. */"]
        for tag in ENUM_PREFIX:
            for name, _ in enums[tag]:
                lines.append("ABI_ENUM(%s, %s)" % (const_name(tag, name), name))
        with open(os.path.join(HERE, "abi_enums_gen.h"), "w") as f:
            f.write("\n".join(lines) + "\n")

    sys.stdout.write("TAP version 13\n1..%d\n%s\n" % (state["n"], "\n".join(out)))
    if state["bad"]:
        sys.stdout.write("# FAILED %d of %d checks\n" % (state["bad"], state["n"]))
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())

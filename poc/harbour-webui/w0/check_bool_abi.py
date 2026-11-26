#!/usr/bin/env python3
"""W0 group B (informational): how do the bool-returning functions end?

For every bool-returning function of include/webui.h, disassemble it from
dist/libwebui-2-static.a (objdump, x86-64 only) and classify the last instruction that
writes rax before each ret or tail jump. The bridge reads such results as unsigned char,
so only the low byte matters; this report shows where upper bytes may be undefined
(setcc/mov to %al, copied registers, tail calls). It cannot fail a build: B never gates W0.
"""

import os
import re
import shutil
import subprocess
import sys

import hdrparse

LIB = os.path.join(hdrparse.REPO, "dist", "libwebui-2-static.a")
SKIP_MNEMONICS = ("add", "sub", "pop", "leave", "nop", "endbr64", "xchg", "data16", "cs", "ds", "lea", "mov%rsp")


def bool_functions():
    names = []
    for (name, ret, _args), _raw in hdrparse.header_protos():
        if ret == "ubool":
            names.append(name)
    return names


def disassemble(names):
    out = subprocess.check_output(["objdump", "-d", "--no-show-raw-insn", LIB], universal_newlines=True, errors="replace")
    funcs, cur, cur_name = {}, None, None
    for line in out.splitlines():
        m = re.match(r"^[0-9a-f]+ <(\w+)>:$", line)
        if m:
            cur_name = m.group(1)
            cur = [] if cur_name in names else None
            if cur is not None:
                funcs[cur_name] = cur
            continue
        if cur is not None:
            m = re.match(r"^\s+([0-9a-f]+):\s+(.*)$", line)
            if m:
                cur.append(m.group(2).strip())
    return funcs


def writes_rax(insn):
    """Return (dest_reg, mnemonic, operands) if the instruction defines rax/eax/ax/al."""
    parts = insn.split(None, 1)
    mnem = parts[0]
    ops = parts[1].split("#")[0].strip() if len(parts) > 1 else ""
    if mnem.startswith("set"):
        return ("al", mnem, ops)
    if "," not in ops:
        return None
    dst = ops.rsplit(",", 1)[1].strip()
    if dst in ("%al", "%ax", "%eax", "%rax"):
        if mnem.startswith(("cmp", "test")):
            return None
        return (dst[1:], mnem, ops)
    return None


def classify(insns, idx):
    """Classify the exit at insns[idx] (a ret or a jmp)."""
    ins = insns[idx]
    if ins.startswith("jmp"):
        return "tail call"
    j = idx - 1
    while j >= 0:
        i = insns[j]
        mnem = i.split(None, 1)[0]
        if mnem in ("add", "sub", "pop", "leave", "nop", "endbr64", "xchg", "data16", "cs", "ds"):
            j -= 1
            continue
        break
    if j < 0:
        return "no writer found"
    w = writes_rax(insns[j])
    if w is None:
        if insns[j].startswith(("call", "jmp")):
            return "result of a call"
        return "other"
    dst, mnem, ops = w
    src = ops.rsplit(",", 1)[0].strip()
    if dst == "al":
        return "low byte only (%s)" % mnem
    if mnem == "xor" and src == "%eax":
        return "constant 0"
    if mnem.startswith("movz") or (mnem == "and" and src in ("$0x1", "$0xff")):
        return "zero-extended"
    if mnem == "mov" and src.startswith("$"):
        return "constant"
    if mnem == "mov" and re.match(r"^%[a-z0-9]+$", src):
        return "copied from register %s (upper bytes as in source)" % src
    if mnem == "mov":
        return "loaded from memory"
    return "computed (%s)" % mnem


def main():
    out, n, bad = [], [0], [0]

    def line(desc, skip=None):
        n[0] += 1
        if skip:
            out.append("ok %d - %s # SKIP %s" % (n[0], desc, skip))
        else:
            out.append("ok %d - %s # informational" % (n[0], desc))

    names = bool_functions()
    problem = None
    if shutil.which("objdump") is None:
        problem = "objdump not installed"
    elif not os.path.exists(LIB):
        problem = "dist/libwebui-2-static.a missing (run make at the repo root)"
    elif subprocess.check_output(["uname", "-m"], universal_newlines=True).strip() != "x86_64":
        problem = "not x86-64"
    if problem:
        line("B1 bool epilogue inspection", skip=problem)
    else:
        funcs = disassemble(set(names))
        missing = [x for x in names if x not in funcs]
        line("B1 %d bool-returning functions in the header, %d found in the archive%s" % (
            len(names), len(funcs), (" (missing: %s)" % ", ".join(missing)) if missing else ""))
        totals = {}
        for name in names:
            insns = funcs.get(name)
            if not insns:
                continue
            classes = {}
            for idx, ins in enumerate(insns):
                is_ret = re.match(r"^(repz |bnd )?ret", ins)
                is_tail = ins.startswith("jmp") and "<" in ins and ("<%s+" % name) not in ins and ("<%s>" % name) not in ins
                if is_ret or is_tail:
                    c = classify(insns, idx)
                    classes[c] = classes.get(c, 0) + 1
                    totals[c] = totals.get(c, 0) + 1
            line("B1 %s: %s" % (name, "; ".join("%s x%d" % (k, v) for k, v in sorted(classes.items())) or "no exit found"))
        line("B1 totals: %s" % "; ".join("%s x%d" % (k, v) for k, v in sorted(totals.items())))
        low = sum(v for k, v in totals.items() if k.startswith(("low byte", "copied", "tail call", "result of")))
        line("B1 %d exits may leave bits 8..63 of rax undefined; the bridge reads only the low byte (unsigned char)" % low)
    sys.stdout.write("TAP version 13\n1..%d\n%s\n" % (n[0], "\n".join(out)))
    return 0


if __name__ == "__main__":
    sys.exit(main())

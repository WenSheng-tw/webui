"""Shared parsing helpers for the PoC-002 W0 tools (Python 3, standard library only)."""

import os
import re

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
HEADER = os.path.join(REPO, "include", "webui.h")

TYPE_WORDS = frozenset(
    "void char int short long unsigned signed float double size_t bool ubool "
    "webui_event_t webui_config const struct enum".split()
)


def strip_comments(text):
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    return re.sub(r"//[^\n]*", "", text)


def split_top(s):
    """Split a parameter list on commas that are not inside parentheses."""
    parts, depth, cur = [], 0, []
    for ch in s:
        if ch == "(":
            depth += 1
        elif ch == ")":
            depth -= 1
        if ch == "," and depth == 0:
            parts.append("".join(cur))
            cur = []
        else:
            cur.append(ch)
    tail = "".join(cur).strip()
    if tail or parts:
        parts.append(tail)
    return [p.strip() for p in parts]


def norm_type(t, bool_as):
    """Normalize one parameter or return type; bool_as is the replacement for bool."""
    t = " ".join(t.split())
    m = re.match(r"^(.*?)\(\s*\*\s*\w*\s*\)\s*\((.*)\)\s*$", t)
    if m:
        ret = norm_type(m.group(1), "ubool")
        args = [norm_type(a, "int") for a in split_top(m.group(2))]
        if args == ["void"] or args == [""]:
            args = ["void"]
        return "%s (*)(%s)" % (ret, ", ".join(args))
    toks = re.findall(r"\w+|\*", t)
    if len(toks) >= 2 and re.match(r"\w+$", toks[-1]) and toks[-1] not in TYPE_WORDS:
        toks = toks[:-1]
    out = []
    for tok in toks:
        if tok == "bool":
            tok = bool_as
        elif tok == "webui_config":
            tok = "int"
        out.append(tok)
    s = " ".join(out).replace(" *", "*").replace("* ", "*")
    s = s.replace("long long int", "long long")
    return s


def parse_proto(text):
    """Return (name, ret, [params]) normalized to bridge conventions, or None."""
    text = " ".join(text.split())
    m = re.match(r"^(.*?)\b(webui_\w+)\s*\((.*)\)\s*$", text)
    if not m:
        return None
    ret = norm_type(m.group(1), "ubool")
    args = [norm_type(a, "int") for a in split_top(m.group(3))]
    if args in ([], [""], ["void"]):
        args = ["void"]
    return m.group(2), ret, args


def header_protos(path=HEADER):
    with open(path) as f:
        text = strip_comments(f.read())
    raw = [" ".join(p.split()) for p in re.findall(r"WEBUI_EXPORT\s+([^;{]+?\))\s*;", text, flags=re.S)]
    out = []
    for r in raw:
        p = parse_proto(r)
        if p is None:
            raise ValueError("cannot parse prototype: " + r)
        out.append((p, r))
    return out


def decl_protos(path):
    """Parse 'extern ... name(...);' declarations from a bridge-side C file."""
    with open(path) as f:
        text = strip_comments(f.read())
    res = {}
    for m in re.finditer(r"(?m)^\s*extern\s+([^;{]+?\))\s*;", text):
        p = parse_proto(m.group(1))
        if p and p[0].startswith("webui_"):
            res[p[0]] = p
    return res


def header_enums(path=HEADER):
    """Return {enum_tag_or_typedef: [(enumerator, value)]} with implicit numbering."""
    with open(path) as f:
        text = strip_comments(f.read())
    res = {}
    for m in re.finditer(r"\b(?:typedef\s+)?enum\s*(\w*)\s*\{([^}]*)\}\s*(\w*)\s*;", text):
        tag = m.group(1) or m.group(3)
        items, val = [], 0
        for item in m.group(2).split(","):
            item = item.strip()
            if not item:
                continue
            if "=" in item:
                name, expr = [x.strip() for x in item.split("=", 1)]
                val = int(expr, 0)
            else:
                name = item
            items.append((name, val))
            val += 1
        res[tag] = items
    return res


def read_functions(path=None):
    path = path or os.path.join(HERE, "functions.txt")
    rows = []
    with open(path) as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#") or line.startswith("name,"):
                continue
            name, wave, area = [x.strip() for x in line.split(",")]
            rows.append((name, wave, area))
    return rows

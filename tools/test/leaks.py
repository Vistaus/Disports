#!/usr/bin/env python3
"""Summarises sanitizer reports (build/test/*/asan.*, ubsan.*).

Leaks are grouped by where they were allocated. A direct leak is ours
when our code (src/: the disports binary, other than main()) is
among the first frames below the allocator, i.e. our code allocated it;
otherwise it is put down to the first library below the allocator. Prints
one line per group, ours first; exits 1 when there is anything of ours
(or any other error / UB report).

usage: leaks.py <report files...>
"""
import re
import sys
from collections import defaultdict

ALLOC = re.compile(r"(operator new|malloc|calloc|realloc|strdup|asan_)")
# "#3 0x7f.. in QFreetypeFace::cleanup() (/lib/libQt6Gui.so.6+0x..)",
# "#9 0x55.. in main /home/.../src/main.cpp:42"
FRAME = re.compile(r"#\d+ 0x[0-9a-f]+ in (.+?)(?: \(/| /| \.\./| \(BuildId|$)")
ENTRY = ("main", "_start", "__libc_start")
# How many frames below the allocator count as "who allocated".
ALLOCATOR_DEPTH = 4


def frames(block):
    out = []
    for line in block.splitlines():
        m = FRAME.search(line)
        if m:
            out.append((m.group(1).strip(), line))
    return out


def ours(func, line):
    if func.startswith(ENTRY) or "/third_party/" in line:
        return False
    return "/app/bin/disports" in line or "/Disports/src/" in line


def summarise(paths):
    leaks = defaultdict(lambda: [0, 0, ""])   # key -> [bytes, count, example]
    other = []
    for path in paths:
        text = open(path, errors="replace").read()
        for block in re.split(r"\n(?=(?:Direct|Indirect) leak of )", text):
            head = re.match(r"(Direct|Indirect) leak of (\d+) byte", block)
            if not head:
                if "ERROR: AddressSanitizer" in block or "runtime error" in block:
                    other.append((path, block.strip()[:2000]))
                continue
            fs = [f for f in frames(block) if not ALLOC.search(f[0])]
            # Ours: our code made the allocation (near the top of the stack),
            # not just called into a library that did. Indirect leaks are
            # owned by a leaked object, and go with its direct leak.
            mine = [f for f in fs[:ALLOCATOR_DEPTH] if ours(*f)]
            if mine and head.group(1) == "Direct":
                key = "OURS  " + mine[0][0]
            else:
                lib = next((re.search(r"\(([^()+]+)\+0x", l) for _, l in fs if re.search(r"\(([^()+]+)\+0x", l)), None)
                key = "lib   " + (lib.group(1).split("/")[-1] if lib else "?") + "  " + (fs[0][0] if fs else "?")
            entry = leaks[key]
            entry[0] += int(head.group(2))
            entry[1] += 1
            entry[2] = entry[2] or block.strip()
    bad = bool(other)
    for key in sorted(leaks, key=lambda k: (not k.startswith("OURS"), -leaks[k][0])):
        size, count, example = leaks[key]
        print("%-6d bytes in %3d leak(s): %s" % (size, count, key))
        if key.startswith("OURS"):
            bad = True
            print("\n".join("        " + l for l in example.splitlines()[:14]))
    for path, block in other:
        print("REPORT in %s:\n%s" % (path, "\n".join("        " + l for l in block.splitlines()[:25])))
    return bad


if __name__ == "__main__":
    sys.exit(1 if summarise(sys.argv[1:]) else 0)

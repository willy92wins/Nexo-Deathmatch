"""F1a: mechanical rename LFGungame -> NexoDM (byte-preserving except the replaced tokens).

Replaces, case-sensitive: LFGungame -> NexoDM, LFGUNGAME -> NEXODM, lfgungame -> nexodm
in every file under the mod folder, renames files/dirs whose names contain those tokens,
and finally renames the mod root folder LFGungame/ -> NexoDM/.
Usage: python rename_lfgungame.py <repo_root> [--apply]
"""
import os, sys, pathlib

PAIRS = [(b"LFGungame", b"NexoDM"), (b"LFGUNGAME", b"NEXODM"), (b"lfgungame", b"nexodm")]
SPAIRS = [(a.decode(), b.decode()) for a, b in PAIRS]

root = pathlib.Path(sys.argv[1])
apply = "--apply" in sys.argv
mod = root / "LFGungame"
total = 0
for p in sorted(mod.rglob("*")):
    if p.is_file():
        data = p.read_bytes()
        new = data
        n = 0
        for a, b in PAIRS:
            n += new.count(a)
            new = new.replace(a, b)
        if n:
            total += n
            print(f"{n:4d}  {p.relative_to(root)}")
            if apply:
                p.write_bytes(new)
# rename deepest paths first
paths = sorted((p for p in mod.rglob("*")), key=lambda p: len(p.parts), reverse=True)
for p in paths:
    name = p.name
    for a, b in SPAIRS:
        name = name.replace(a, b)
    if name != p.name:
        print("mv", p.relative_to(root), "->", name)
        if apply:
            p.rename(p.with_name(name))
print("mv LFGungame -> NexoDM")
if apply:
    mod.rename(root / "NexoDM")
print("token replacements:", total)

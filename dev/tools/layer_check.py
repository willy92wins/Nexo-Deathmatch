"""Module-layer check for NexoDM (the offline linter does not catch this).

Enforce compiles 3_Game -> 4_World -> 5_Mission; a lower module cannot see classes declared in a higher one.
This lists every class declared in the vanilla 5_mission / 4_world trees and in NexoDM's own 5_Mission / 4_World,
then reports any use of those names inside NexoDM files of a lower layer (comments stripped).
Exit 0 = no cross-layer use found. Usage: python dev/tools/layer_check.py [vanilla_scripts_root]
"""
import os, re, sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
MOD = os.path.join(ROOT, "NexoDM", "scripts")
VAN = sys.argv[1] if len(sys.argv) > 1 else r"P:\scripts"
CLASS = re.compile(r"^\s*(?:modded\s+)?class\s+(\w+)", re.M)


def classes_in(folder):
    names = set()
    for base, _, files in os.walk(folder):
        for f in files:
            if f.lower().endswith(".c"):
                txt = open(os.path.join(base, f), encoding="utf-8", errors="replace").read()
                names |= set(CLASS.findall(txt))
    return names


def strip(txt):
    txt = re.sub(r"/\*.*?\*/", "", txt, flags=re.S)
    txt = re.sub(r"//[^\n]*", "", txt)
    return re.sub(r'"(?:\\.|[^"\\])*"', '""', txt)


van = {layer: classes_in(os.path.join(VAN, layer)) for layer in ("3_game", "4_world", "5_mission")}
mod = {layer: classes_in(os.path.join(MOD, layer)) for layer in ("3_Game", "4_World", "5_Mission")}
higher = {
    "3_Game": (van["4_world"] | van["5_mission"] | mod["4_World"] | mod["5_Mission"]) - van["3_game"] - mod["3_Game"],
    "4_World": (van["5_mission"] | mod["5_Mission"]) - van["3_game"] - van["4_world"] - mod["3_Game"] - mod["4_World"],
}
hits = 0
for layer, forbidden in higher.items():
    for base, _, files in os.walk(os.path.join(MOD, layer)):
        for f in files:
            if not f.endswith(".c"):
                continue
            path = os.path.join(base, f)
            code = strip(open(path, encoding="utf-8", errors="replace").read())
            for name in sorted(forbidden):
                for m in re.finditer(r"\b%s\b" % re.escape(name), code):
                    line = code.count("\n", 0, m.start()) + 1
                    print(f"LAYER {layer} uses {name}: {os.path.relpath(path, ROOT)}:{line}")
                    hits += 1
print(f"layer_check: {hits} cross-layer uses (vanilla classes: 3_game={len(van['3_game'])} 4_world={len(van['4_world'])} 5_mission={len(van['5_mission'])})")
sys.exit(1 if hits else 0)

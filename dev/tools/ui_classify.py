"""Classify ui_reconcile findings: concatenated names (all indices exist), vanilla HUD widgets, or really missing."""
import re, subprocess, sys, pathlib
REPO = pathlib.Path(r"C:\Users\guill\Repos\Nexo-Deathmatch")
UI = r"C:\Users\guill\DayZ-Modding-Knowledge-Pack\tools\dayz-script-validator\scripts\ui_reconcile.py"
p = subprocess.run([sys.executable, UI, str(REPO / "NexoDM")], capture_output=True, text=True, encoding="utf-8", errors="replace")
mod_layouts = "\n".join(f.read_text(encoding="utf-8", errors="replace") for f in (REPO / "NexoDM/gui/layouts").glob("*.layout"))
van = pathlib.Path(r"P:\gui\layouts\day_z_hud.layout").read_text(encoding="utf-8", errors="replace")
for line in p.stdout.splitlines():
    m = re.search(r"\[(FAIL|WARN)\]\s+(\S+)\s+(\S+):(\d+)", line)
    if not m:
        continue
    sev, rule, f, ln = m.groups()
    src = (REPO / "NexoDM" / f).read_text(encoding="utf-8", errors="replace").splitlines()[int(ln) - 1]
    nm = re.search(r'FindAnyWidget\(\s*"([^"]+)"\s*(\+)?', src)
    if not nm:
        print(sev, rule, f"{f}:{ln}", "?", src.strip()[:100]); continue
    name, concat = nm.group(1), bool(nm.group(2))
    if concat:
        idx = sorted(set(re.findall(r"\b%s(\d+)\b" % re.escape(name), mod_layouts)), key=int)
        verdict = "CONCAT indices=" + ",".join(idx)
    elif re.search(r"\b%s\b" % re.escape(name), van):
        verdict = "VANILLA day_z_hud.layout"
    elif re.search(r"\b%s\b" % re.escape(name), mod_layouts):
        verdict = "IN-MOD-LAYOUT"
    else:
        verdict = "MISSING"
    print(sev, rule, f"{f}:{ln}", name, verdict)

"""Propose NexoDM spawn points in vanilla ChernarusPlus zones.
Sources:
  - zone centres: P:\\DZ\\worlds\\chernarusplus\\world\\config.cpp (CfgWorlds Names, position[])
  - ground points: road/sidewalk object instances parsed from P:\\DZ\\worlds\\chernarusplus\\world\\chernarusplus.wrp
    (wrp_object_parser.py --roads-only) -> pos [x, y, z]; y = object origin on the road surface
  - building clearance: dayzOffline.chernarusplus\\mapgrouppos.xml (vanilla mission, Steam DayZServer)
"""
import json, math, re, sys

ROADS = sys.argv[1]
MGP = r"C:\Program Files (x86)\Steam\steamapps\common\DayZServer\mpmissions\dayzOffline.chernarusplus\mapgrouppos.xml"
ZONES = [
    ("Kamyshovo", 12170.0, 3458.0, "Settlement_Kamyshovo"),
    ("Vybor", 3909.8311, 9019.6143, "Settlement_Vybor"),
    ("Pavlovo_Militar", 2016.0, 3408.0, "Local_MB_Pavlovo"),
    ("Grishino", 5980.0, 10257.0, "Settlement_Grishino"),
]
R_MIN, R_MAX = 20.0, 110.0
CLEAR_BLD = 9.0
SPREAD = 10.0
GROUPS, PER = 3, 5

roads = [o for o in json.load(open(ROADS, encoding="utf-8"))["objects"] if not o["is_bridge"]]
bld = []
rx = re.compile(r'name="([^"]+)" pos="([-\d.]+) ([-\d.]+) ([-\d.]+)"')
for line in open(MGP, encoding="utf-8", errors="replace"):
    m = rx.search(line)
    if m:
        bld.append((m.group(1), float(m.group(2)), float(m.group(3)), float(m.group(4))))

out = []
for name, cx, cz, cls in ZONES:
    near_b = [b for b in bld if math.hypot(b[1] - cx, b[3] - cz) < R_MAX + 40]
    cand = []
    for o in roads:
        x, y, z = o["pos"]
        r = math.hypot(x - cx, z - cz)
        if not (R_MIN <= r <= R_MAX):
            continue
        dmin = min((math.hypot(b[1] - x, b[3] - z) for b in near_b), default=99)
        if dmin < CLEAR_BLD:
            continue
        ang = (math.degrees(math.atan2(z - cz, x - cx)) + 360) % 360
        cand.append((x, y, z, ang, dmin, o["p3d"].rsplit("\\", 1)[-1]))
    groups = []
    # farthest-first seeds, then assign each candidate to its nearest seed
    seeds = [max(cand, key=lambda c: c[4])]
    while len(seeds) < GROUPS:
        seeds.append(max(cand, key=lambda c: min(math.hypot(c[0] - s[0], c[2] - s[2]) for s in seeds)))
    buckets = [[] for _ in seeds]
    for c in cand:
        k = min(range(len(seeds)), key=lambda i: math.hypot(c[0] - seeds[i][0], c[2] - seeds[i][2]))
        buckets[k].append(c)
    for g in range(GROUPS):
        sec = sorted(buckets[g], key=lambda c: -c[4])
        pick = []
        for c in sec:
            if all(math.hypot(c[0] - p[0], c[2] - p[2]) >= SPREAD for p in pick):
                pick.append(c)
            if len(pick) == PER:
                break
        groups.append(pick)
    ys = [c[1] for c in cand]
    print(f"== {name} ({cls} {cx},{cz}) buildings={len(near_b)} candidates={len(cand)} "
          f"Y {min(ys):.1f}-{max(ys):.1f}" if ys else f"== {name}: no candidates")
    for i, gp in enumerate(groups):
        print(f"  group {i+1} ({len(gp)}):", ["%.1f,%.1f,%.1f" % (p[0], p[1], p[2]) for p in gp],
              "clear", [round(p[4], 1) for p in gp], "p3d", sorted({p[5] for p in gp}))
    out.append({"Name": name, "World": "ChernarusPlus", "SpawnRadius": 3,
                "SpawnGroups": [{"Name": f"{name}_{'NES'[i] if False else i+1}",
                                 "Points": ["%.1f,%.1f,%.1f" % (p[0], p[1], p[2]) for p in gp]}
                                for i, gp in enumerate(groups)]})
json.dump(out, open(sys.argv[2], "w", encoding="utf-8"), indent=2)

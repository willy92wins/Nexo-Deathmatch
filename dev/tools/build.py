"""Pack NexoDM/ into build/@NexoDM/addons/NexoDM.pbo with AddonBuilder -packonly (no binarize, no signing).

Then checks that every source file is inside the PBO and nothing unexpected was added.
Exit 0 = packed and content matches. Signing is done by the server owner with their own key.
Usage: python dev/tools/build.py
"""
import os, struct, subprocess, sys, tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SRC = os.path.join(ROOT, "NexoDM")
DEST = os.path.join(ROOT, "build", "@NexoDM", "addons")
TOOLS = os.environ.get("DAYZ_TOOLS_BIN", r"C:\Program Files (x86)\Steam\steamapps\common\DayZ Tools\Bin")
ADDON_BUILDER = os.path.join(TOOLS, "AddonBuilder", "AddonBuilder.exe")
GENERATED = {"$pboprefix$", "$pboprefix$.txt", "texheaders.bin"}
SKIP_SOURCE = {"$pboprefix$"}


def pbo_entries(path):
    names = []
    with open(path, "rb") as f:
        def rz():
            b = b""
            while True:
                c = f.read(1)
                if c in (b"\0", b""):
                    return b.decode("utf-8", "replace")
                b += c
        while True:
            name = rz()
            mime, _o, _r, _t, _s = struct.unpack("<5I", f.read(20))
            if name == "" and mime == 0x56657273:
                while rz():
                    pass
                continue
            if name == "":
                break
            names.append(name.replace("\\", "/").lower())
    return names


def source_files(src):
    out = []
    for base, _, names in os.walk(src):
        for n in names:
            if n.lower() in SKIP_SOURCE:
                continue
            out.append(os.path.relpath(os.path.join(base, n), src).replace("\\", "/").lower())
    return out


def main():
    prefix = open(os.path.join(SRC, "$PBOPREFIX$"), encoding="ascii").read().strip()
    os.makedirs(DEST, exist_ok=True)
    temp = tempfile.mkdtemp(prefix="nexodm_build_")
    cmd = [ADDON_BUILDER, SRC, DEST, "-packonly", "-clear", f"-prefix={prefix}", f"-temp={temp}"]
    r = subprocess.run(cmd, capture_output=True, text=True)
    pbo = os.path.join(DEST, "NexoDM.pbo")
    print("AddonBuilder rc", r.returncode)
    if r.returncode != 0 or not os.path.isfile(pbo):
        print(r.stdout[-2000:], r.stderr[-2000:])
        sys.exit(f"FAIL pack rc={r.returncode}")
    entries = set(pbo_entries(pbo))
    expected = set(source_files(SRC))
    missing = sorted(expected - entries)
    extra = sorted(e for e in entries - expected if e.split("/")[-1] not in GENERATED)
    if missing or extra:
        sys.exit(f"FAIL content: missing={missing[:10]} extra={extra[:10]}")
    print(f"ok NexoDM: {len(expected)} files, {os.path.getsize(pbo)} bytes, prefix {prefix}, {pbo}")


if __name__ == "__main__":
    main()

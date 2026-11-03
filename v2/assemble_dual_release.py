#!/usr/bin/env python3
"""Reproducible FITD-R36S dual-firmware PortMaster ZIP assembler.

Inputs are the two successful workflow *inner* ZIPs from Actions, not
commercial Alone in the Dark game data. Run on Linux/WSL with readelf.
"""
import argparse
import hashlib
import json
import re
import subprocess
import tempfile
from pathlib import Path
from zipfile import ZipFile, ZipInfo, ZIP_DEFLATED

MODERN = "fitd.aarch64"
LEGACY = "fitd.glibc230.aarch64"
LEGACY_LIBS = ("libatomic.so.1", "libgcc_s.so.1", "libstdc++.so.6")


def extract(zip_path):
    with ZipFile(zip_path) as z:
        if z.testzip() is not None:
            raise SystemExit("Corrupt input ZIP: " + str(zip_path))
        files = {}
        for e in z.infolist():
            name = e.filename
            if name.startswith("/") or ".." in Path(name).parts or name in files:
                raise SystemExit("Unsafe/duplicate ZIP path: " + name)
            if e.is_dir():
                continue
            files[name] = (z.read(name), (e.external_attr >> 16) & 0o777)
        return files


def require(items, name):
    if name not in items:
        raise SystemExit("Missing required file: " + name)
    return items[name][0]


def abi(name, contents, limit):
    with tempfile.TemporaryDirectory() as tmp:
        p = Path(tmp) / "elf"
        p.write_bytes(contents)
        header = subprocess.check_output(["readelf", "-h", str(p)], text=True)
        if not re.search(r"Machine:\s+AArch64\b", header):
            raise SystemExit("Not AArch64: " + name)
        sym = subprocess.check_output(["readelf", "-W", "-V", str(p)], text=True)
        versions = [
            tuple(map(int, v.split("."))) for v in re.findall(
                r"\bGLIBC_(\d+\.\d+)\b", sym
            )
        ]
        if versions and max(versions) > limit:
            raise SystemExit(
                "{} GLIBC={} exceeds {}".format(name, max(versions), limit)
            )
    print("PASS ABI:", name)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--modern", type=Path, required=True)
    ap.add_argument("--legacy", type=Path, required=True)
    ap.add_argument("--launcher", type=Path,
                    default=Path("v2/port/FITD-R36S-ARM64-RELEASE.sh"))
    ap.add_argument("--readme", type=Path,
                    default=Path("v2/DUAL_FIRMWARE_README.md"))
    ap.add_argument("--output", type=Path,
                    default=Path("work/release/fitdr36s.zip"))
    args = ap.parse_args()

    pkg = extract(args.modern)
    legacy = extract(args.legacy)
    require(pkg, "fitdr36s/" + MODERN)
    require(pkg, "port.json")
    require(pkg, "FITD-R36S-ARM64.sh")
    require(pkg, "gameinfo.xml")
    for lib in LEGACY_LIBS:
        p = "libs.glibc230/" + lib
        require(legacy, p)
        pkg["fitdr36s/" + p] = legacy[p]
    pkg["fitdr36s/" + LEGACY] = legacy[LEGACY]
    if any(p.lower().endswith((".pak", ".itd", ".iso", ".rom")) for p in pkg):
        raise SystemExit("Commercial game files detected; refusing packaging")

    launcher = args.launcher.read_bytes()
    if b'[ "$GLIBC_MINOR" -le 34 ]' not in launcher:
        raise SystemExit("Launcher does not select legacy ABI for glibc <=2.34")
    for term in (b"libs.glibc230", b"fitd.glibc230.aarch64"):
        if term not in launcher:
            raise SystemExit("Launcher missing legacy binary selection")
    pkg["FITD-R36S-ARM64.sh"] = (launcher, 0o755)

    for p in ("fitdr36s/fitd.aarch64", *[
        "fitdr36s/libs.aarch64/" + lib for lib in LEGACY_LIBS
    ]):
        abi(p, require(pkg, p), (2, 35))
    for p in ("fitdr36s/" + LEGACY, *[
        "fitdr36s/libs.glibc230/" + lib for lib in LEGACY_LIBS
    ]):
        abi(p, require(pkg, p), (2, 30))

    meta = json.loads(require(pkg, "port.json"))
    meta["name"] = "fitdr36s.zip"
    meta["items"] = ["FITD-R36S-ARM64.sh", "fitdr36s"]
    meta["attr"]["title"] = "Alone in the Dark (FITD) - R36S ARM64"
    meta["attr"]["desc"] = (
        "Community FITD port for dArkOSRE and ArkOS4Clone. "
        "Requires user's original Alone in the Dark 1 data."
    )
    meta["attr"]["inst"] = (
        "Copy original game files to fitdr36s/data/. "
        "The launcher selects an ARM64 binary based on glibc."
    )
    meta["attr"]["arch"] = ["aarch64"]
    meta["attr"]["min_glibc"] = "2.30"
    meta["attr"]["exp"] = False
    encoded = (json.dumps(meta, indent=2, ensure_ascii=False) + "\n").encode()
    pkg["port.json"] = (encoded, 0o644)
    pkg["fitdr36s/port.json"] = (encoded, 0o644)

    readme = args.readme.read_bytes()
    pkg["README.md"] = (readme, 0o644)
    pkg["fitdr36s/README-FIRMWARES.txt"] = (readme, 0o644)
    pkg["fitdr36s/BUILD-SOURCES.txt"] = (
        b"Modern ARM64: Actions run 41561105602\n"
        b"Legacy ARM64: Actions run 41562095088\n"
        b"Code: https://github.com/dehalbuq/FITD-R36S\n", 0o644
    )
    pkg["fitdr36s/licenses/LEGACY-RUNTIME-NOTICE.txt"] = (
        b"Legacy GNU C++/GCC runtime libraries were built from Debian 10 "
        b"(Buster) system runtime. See libstdc++6-copyright.txt, "
        b"libgcc-s1-copyright.txt and libatomic1-copyright.txt alongside "
        b"the original upstream GCC copyright and runtime exception.\n"
        b"Do not redistribute any commercial Alone in the Dark assets.\n", 0o644
    )

    args.output.parent.mkdir(parents=True, exist_ok=True)
    with ZipFile(args.output, "w", ZIP_DEFLATED, compresslevel=9) as output:
        for path in sorted(pkg):
            content, perm = pkg[path]
            info = ZipInfo(path)
            info.create_system = 3
            info.external_attr = (0o100000 | (perm or 0o644)) << 16
            info.compress_type = ZIP_DEFLATED
            output.writestr(info, content, compress_type=ZIP_DEFLATED,
                            compresslevel=9)
    with ZipFile(args.output) as check:
        if check.testzip() is not None:
            raise SystemExit("Output ZIP corrupted")
    checksum = hashlib.sha256(args.output.read_bytes()).hexdigest()
    (args.output.parent / "SHA256SUMS.txt").write_text(
        checksum + "  " + args.output.name + "\n"
    )
    print("READY:", args.output, checksum, len(pkg), "files")


if __name__ == "__main__":
    main()

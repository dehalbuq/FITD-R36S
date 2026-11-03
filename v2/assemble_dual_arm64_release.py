#!/usr/bin/env python3
"""Merge two independently validated FITD AArch64 Actions builds into one PortMaster ZIP.

Inputs are the inner ZIP files extracted from GitHub Actions artifact wrappers.
No game assets are packaged. Preserve both ABIs and select at launch time.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import zipfile

RELEASE_NAME = "FITD-R36S-v1.0.0-ARM64-PortMaster.zip"

def main():
    p = argparse.ArgumentParser()
    p.add_argument("--modern", type=Path, required=True)
    p.add_argument("--legacy", type=Path, required=True)
    p.add_argument("--launcher", type=Path, required=True)
    p.add_argument("--out", type=Path, required=True)
    args = p.parse_args()
    with tempfile.TemporaryDirectory(prefix="fitd-release-") as temp:
        dest = Path(temp) / "package"
        dest.mkdir()
        for name, src in (("modern", args.modern), ("legacy", args.legacy)):
            with zipfile.ZipFile(src) as z:
                corrupt = z.testzip()
                if corrupt:
                    raise SystemExit("{} ZIP corrupt: {}".format(name, corrupt))
                for member in z.infolist():
                    parts = Path(member.filename).parts
                    if (member.filename.startswith("/") or ".." in parts or
                            member.filename.startswith("\\") or
                            member.is_dir() and not parts):
                        raise SystemExit("Unsafe archive member: " + member.filename)
                if name == "modern":
                    z.extractall(dest)
                else:
                    legacy = Path(temp) / "legacy"
                    z.extractall(legacy)
                    game = dest / "fitdr36s"
                    for filename in ("fitd.glibc230.aarch64", "README-LEGACY.txt"):
                        if not (legacy / filename).is_file():
                            raise SystemExit("Missing legacy file: " + filename)
                        shutil.copy2(legacy / filename, game / filename)
                    source_libs = legacy / "libs.glibc230"
                    if not source_libs.is_dir():
                        raise SystemExit("Missing ABI-compatible legacy libraries")
                    shutil.copytree(source_libs, game / source_libs.name)

        game = dest / "fitdr36s"
        for required in ("fitd.aarch64", "fitd.glibc230.aarch64",
                         "libs.aarch64/libstdc++.so.6",
                         "libs.glibc230/libstdc++.so.6",
                         "libs.glibc230/libgcc_s.so.1",
                         "data/COPY_GAME_FILES_HERE.txt",
                         "LEIA-ME-LEGAL.txt",
                         "LICENSE.FITD.txt"):
            if not (game / required).is_file():
                raise SystemExit("Missing release file: " + required)
        shutil.copy2(args.launcher, dest / "FITD-R36S-ARM64.sh")
        (dest / "FITD-R36S-ARM64.sh").chmod(0o755)
        for binary in ("fitd.aarch64", "fitd.glibc230.aarch64"):
            (game / binary).chmod(0o755)

        for base in (dest, game):
            target = base / "port.json"
            if not target.is_file():
                continue
            metadata = json.loads(target.read_text())
            metadata["name"] = RELEASE_NAME
            metadata.setdefault("attr", {})["min_glibc"] = "2.28"
            metadata["attr"]["desc"] = (
                "ARM64 port with automatic glibc-based selection for "
                "ArkOS4Clone (glibc 2.30) and dArkOSRE. Original game files required."
            )
            metadata["attr"]["exp"] = False
            target.write_text(json.dumps(metadata, indent=2, ensure_ascii=False) + "\n")
        if not (dest / "port.json").is_file():
            raise SystemExit("Missing PortMaster root port.json")
        root_meta = json.loads((dest / "port.json").read_text())
        if root_meta["items"] != ["FITD-R36S-ARM64.sh", "fitdr36s"]:
            raise SystemExit("PortMaster metadata items are unexpected")
        # No bundled proprietary data, especially leftover test saves.
        in_data = [x for x in (game / "data").rglob("*") if x.is_file()]
        if any(x.suffix.lower() in (".pak", ".itd") for x in in_data):
            raise SystemExit("Refusing to distribute original game assets or saves")

        result = args.out / RELEASE_NAME
        args.out.mkdir(parents=True, exist_ok=True)
        with zipfile.ZipFile(result, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as out:
            for f in sorted(dest.rglob("*")):
                if not f.is_file():
                    continue
                rel = f.relative_to(dest).as_posix()
                mode = (0o755 if rel == "FITD-R36S-ARM64.sh" or
                        rel in ("fitdr36s/fitd.aarch64",
                                "fitdr36s/fitd.glibc230.aarch64") else 0o644)
                zi = zipfile.ZipInfo(rel)
                zi.create_system = 3
                zi.external_attr = (0o100000 | mode) << 16
                zi.compress_type = zipfile.ZIP_DEFLATED
                with f.open("rb") as source, out.open(zi, "w", force_zip64=True) as target:
                    shutil.copyfileobj(source, target)
        with zipfile.ZipFile(result) as z:
            if z.testzip():
                raise SystemExit("Final ZIP integrity validation failed")
            names = z.namelist()
            assert "FITD-R36S-ARM64.sh" in names
            assert "fitdr36s/fitd.glibc230.aarch64" in names
            assert "fitdr36s/fitd.aarch64" in names
        digest = hashlib.sha256(result.read_bytes()).hexdigest()
        (args.out / "SHA256SUMS.txt").write_text(digest + "  " + result.name + "\n")
        print("Ready:", result, "SHA256", digest)

if __name__ == "__main__":
    main()

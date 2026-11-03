#!/usr/bin/env python3
"""Reject ARM64 PortMaster binaries whose ABI exceeds the target firmware.

Checks the ELF executable AND every bundled .so; glibc from the firmware is
never redistributed. This is a build-time gate, not proof that EGL/GLES3 works.
"""
import argparse
from pathlib import Path
import re
import subprocess
import sys


def version(value):
    return tuple(int(x) for x in value.split("."))


def readelf(*args):
    return subprocess.check_output(
        ["readelf", *[str(x) for x in args]],
        text=True,
        stderr=subprocess.STDOUT,
    )


def audit(path, glibc_max, glibcxx_max):
    hdr = readelf("-h", path)
    if not re.search(r"Machine:\s+AArch64\b", hdr):
        raise ValueError("{} is not an AArch64 ELF".format(path))
    text = readelf("-W", "-V", path)
    requires = {}
    for prefix in ("GLIBC", "GLIBCXX", "CXXABI"):
        values = [version(x) for x in re.findall(
            r"\b" + prefix + r"_([0-9]+(?:\.[0-9]+)+)\b", text
        )]
        requires[prefix] = max(values) if values else ()
    print("{}: GLIBC={} GLIBCXX={} CXXABI={}".format(
        path, *((".".join(map(str, requires[k])) or "none")
                for k in ("GLIBC", "GLIBCXX", "CXXABI"))
    ))
    if requires["GLIBC"] > glibc_max:
        raise ValueError("{} requires GLIBC {}, target maximum {}".format(
            path, requires["GLIBC"], glibc_max))
    if requires["GLIBCXX"] > glibcxx_max:
        raise ValueError("{} requires GLIBCXX {}, target maximum {}".format(
            path, requires["GLIBCXX"], glibcxx_max))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--max-glibc", default="2.30")
    parser.add_argument("--max-glibcxx", default="3.4.28")
    parser.add_argument("files", nargs="+")
    args = parser.parse_args()
    targets = []
    for given in map(Path, args.files):
        if given.is_dir():
            targets.extend(sorted(p for p in given.iterdir() if p.is_file()))
        else:
            targets.append(given)
    if not targets:
        raise SystemExit("No ELF binaries found for ABI audit")
    for file in targets:
        if not file.is_file():
            raise SystemExit("Missing ELF: " + str(file))
        try:
            audit(file, version(args.max_glibc), version(args.max_glibcxx))
        except (subprocess.CalledProcessError, ValueError) as exc:
            raise SystemExit("ABI AUDIT FAILED: {}".format(exc))
    print("ABI AUDIT PASSED: checked {} AArch64 ELF files".format(len(targets)))


if __name__ == "__main__":
    main()

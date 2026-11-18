#!/usr/bin/env python3
"""Restore missing explicit HQR_Reset<sBody> instantiation at ARMHF link time.

Source: the upstream FITD hqr.cpp defines the template in a .cpp file, so
floor.cpp cannot instantiate it. Explicit instantiation emits the linker
symbol while preserving the original HQR_Reset cleanup semantics.
This patch is idempotent and changes no game logic.
"""
from pathlib import Path
import sys

if len(sys.argv) != 2:
    raise SystemExit("usage: patch_hqr_reset_body.py FITD_SOURCE_DIR")
path = Path(sys.argv[1]) / "FitdLib" / "hqr.cpp"
if not path.is_file():
    raise SystemExit(f"Missing upstream file: {path}")

raw = path.read_bytes()
anchor = b"template void HQR_Free(hqrEntryStruct<sBody>* hqrPtr);"
addition = b"template void HQR_Reset(hqrEntryStruct<sBody>* hqrPtr);"

if raw.count(addition) == 1:
    print("[FITD-LINK] HQR_Reset<sBody> already explicitly instantiated")
    sys.exit(0)
if raw.count(addition) != 0 or raw.count(anchor) != 1:
    raise SystemExit("[FITD-LINK] Upstream hqr.cpp changed; refusing unsafe patch")
newline = b"\r\n" if b"\r\n" in raw else b"\n"
raw = raw.replace(anchor, anchor + newline + addition, 1)
path.write_bytes(raw)
print("[FITD-LINK] Fixed missing HQR_Reset<sBody> symbol in", path)

#!/usr/bin/env python3
"""Disable diagnostics only, after all gameplay and crash-safety patches."""
from pathlib import Path
import sys

if len(sys.argv) != 2:
    raise SystemExit("usage: prepare_release_arm64.py SOURCE_DIR")
p = Path(sys.argv[1]) / "FitdLib/r36sCrashDebug.h"
s = p.read_text()
def once(old, new):
    global s
    n = s.count(old)
    if n != 1:
        raise RuntimeError(f"{p}: expected one anchor, found {n}: {old!r}")
    s = s.replace(old, new, 1)
once("#ifdef FITD_R36S\n#include <csignal>", "#if defined(FITD_R36S) && !defined(FITD_R36S_RELEASE)\n#include <csignal>")
once("#else\n#define R36S_TRACE(...) do {} while (0)", "#else\ninline bool r36sDeepTrace() { return false; }\n#define R36S_TRACE(...) do {} while (0)")
p.write_text(s)
print("Release: no signal backtraces or deep trace; all gameplay fixes retained")

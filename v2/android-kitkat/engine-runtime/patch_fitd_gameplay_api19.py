#!/usr/bin/env python3
"""Source-pinned Android 4.4/SDL2 compatibility edits for upstream FITD.

Only applies to temporary clone of yaz0r/FITD commit
543e22bc78b64405537ae545ecd382dcdf2c12b2. Never patches R36S
stable source or GitHub upstream in place.
"""
from pathlib import Path
import sys

if len(sys.argv)!=2:
    raise SystemExit("usage: patch_fitd_gameplay_api19.py /tmp/fitd-upstream")

root=Path(sys.argv[1])
def one(path, old, new, reason):
    f=root/path
    s=f.read_text()
    count=s.count(old)
    if count!=1:
        raise RuntimeError(f"{reason}: {path} found {count}, expected exactly 1")
    f.write_text(s.replace(old,new,1))
    print("FITD-API19:",reason)

# The upstream debug bounding-volume display escapes its FITD_DEBUGGER guard.
# It calls drawZv when the debug-only backgroundMode enum exists. This must
# be excluded from an actual non-debug release, not re-enable debug globally.
one("FitdLib/animAction.cpp",
    '''            if (backgroundMode == backgroundModeEnum_3D) {
                drawZv(rangeZv);
            }''',
    '''#ifdef FITD_DEBUGGER
            if (backgroundMode == backgroundModeEnum_3D) {
                drawZv(rangeZv);
            }
#endif''',
    "exclude debugger-only action collision volume overlay")

# Original hqr.cpp instantiates HQR_Reset<char> and HQR_Reset<sAnimation>,
# but forgets HQR_Reset<sBody>. The release game references that symbol;
# explicit instantiation is required by the C++ linker on Android ARMv7.
one("FitdLib/hqr.cpp",
    'template void HQR_Free(hqrEntryStruct<sBody>* hqrPtr);',
    'template void HQR_Free(hqrEntryStruct<sBody>* hqrPtr);\n'
    'template void HQR_Reset(hqrEntryStruct<sBody>* hqrPtr);',
    "instantiate original body resource cache reset for final link")

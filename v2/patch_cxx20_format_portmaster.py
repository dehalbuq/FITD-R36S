#!/usr/bin/env python3
"""Replace FITD's std::format PAK filename formatting for GCC 11 / Ubuntu 22.04.

The AArch64 PortMaster build deliberately targets glibc 2.35. Ubuntu 22.04's
GCC 11 libstdc++ does not provide <format>; using a newer toolchain would
raise the libstdc++ runtime baseline on older handheld firmwares.
Only numeric PAK filename patterns are currently used by upstream FITD.
"""
from pathlib import Path
import re
import sys

if len(sys.argv) != 2:
    raise SystemExit("usage: patch_cxx20_format_portmaster.py SOURCE_DIR")

root = Path(sys.argv[1]) / "FitdLib"
config = root / "config.h"
source = config.read_text()
old = "#include <format>"
if source.count(old) != 1:
    raise SystemExit(f"{config}: expected one <format> include; found {source.count(old)}")
helper = """#include <cstdio>
// PortMaster / Ubuntu 22.04: GCC 11 lacks C++20 <format>. FITD only uses
// std::format for 2-digit floor/camera PAK filenames, so snprintf suffices.
inline std::string r36sFormatIndexedFilename(const char* prefix, int index,
                                            const char* suffix = "")
{
    char name[64];
    std::snprintf(name, sizeof(name), "%s%02d%s", prefix, index, suffix);
    return std::string(name);
}
"""
source = source.replace(old, helper)
config.write_text(source)

pat = re.compile(
    r'std::format\("([A-Z]+)\{:02d\}(\.PAK)?",\s*(g_currentFloor|floorNumber)\)'
)
total = 0
for rel in ("room.cpp", "floor.cpp"):
    path = root / rel
    src = path.read_text()
    def replacement(match):
        prefix, extension, variable = match.groups()
        return (f'r36sFormatIndexedFilename("{prefix}", {variable}, '
                f'"{extension or ""}")')
    updated, count = pat.subn(replacement, src)
    if count < 1 or "std::format" in updated:
        raise SystemExit(
            f"{path}: replaced {count} occurrences; unsupported std::format remains"
        )
    total += count
    path.write_text(updated)

print(f"PortMaster GCC 11 compatibility: replaced {total} std::format calls")

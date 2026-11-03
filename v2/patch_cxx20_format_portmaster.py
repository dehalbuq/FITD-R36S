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

# The upstream engine also defines "byte" as a global macro in osystem.h.
# GCC11's <functional> declares "enum class byte", which that macro destroys
# when osystemAL.cpp includes <algorithm> after common.h. Remove the macro
# and retain the exact original parameter types in declarations.
header = root / "osystem.h"
source = header.read_text()
if source.count("#define byte char") != 1:
    raise SystemExit(f"{header}: expected one legacy byte macro")
source = source.replace("#define byte char", "// FITD PortMaster: no global byte macro (conflicts with C++17 std::byte)")
declarations = {
    "osystem_setColor(byte i, byte R, byte G, byte B)": "osystem_setColor(char i, char R, char G, char B)",
    "osystem_setPalette(unsigned byte * palette)": "osystem_setPalette(unsigned char * palette)",
    "osystem_setPalette320x200(byte * palette)": "osystem_setPalette320x200(char * palette)",
}
for old, new in declarations.items():
    if source.count(old) != 1:
        raise SystemExit(f"{header}: expected one prototype {old}")
    source = source.replace(old, new, 1)
if re.search(r"\\bbyte\\b", "\\n".join(line for line in source.splitlines()
                                     if not line.strip().startswith("//"))):
    raise SystemExit(f"{header}: unexpected legacy byte token remains")
header.write_text(source)
print("PortMaster GCC 11 compatibility: removed conflicting global byte macro")

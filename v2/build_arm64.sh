#!/usr/bin/env bash
set -Eeuo pipefail
cd "$(dirname "$0")/.."
ROOT="$PWD"
FITD_RELEASE="${FITD_RELEASE:-0}"
case "$FITD_RELEASE" in
  0)
    FITD_BUILD_FLAGS="-O1 -g3 -fno-omit-frame-pointer -fno-optimize-sibling-calls -fstack-protector-strong -D_GLIBCXX_ASSERTIONS -DNDEBUG -march=armv8-a"
    FITD_LINK_FLAGS="-rdynamic"
    FITD_ARCHIVE="fitdr36s-aarch64-startup-credits-legal.zip"
    ;;
  1)
    # Conservatively optimized: no LTO, O2, O3, Ofast or fast-math.
    FITD_BUILD_FLAGS="-O1 -g0 -DNDEBUG -DFITD_R36S_RELEASE=1 -march=armv8-a -mtune=cortex-a35 -fno-strict-aliasing -fwrapv -fstack-protector-strong -D_GLIBCXX_ASSERTIONS -ffunction-sections -fdata-sections"
    FITD_LINK_FLAGS="-Wl,--gc-sections -Wl,--as-needed"
    FITD_ARCHIVE="fitdr36s.zip"
    ;;
  *) echo "FITD_RELEASE must be 0 or 1" >&2; exit 2 ;;
esac
UPSTREAM_SHA="543e22bc78b64405537ae545ecd382dcdf2c12b2"
mkdir -p work
rm -rf work/source work/arm64 work/package
git clone --recursive https://github.com/yaz0r/FITD.git work/source
git -C work/source checkout --detach "$UPSTREAM_SHA"
git -C work/source submodule update --init --recursive
python3 -m py_compile v2/patch_debug_audio_crash.py
python3 v2/patch_upstream.py work/source
python3 v2/patch_debug_audio_crash.py work/source
python3 -m py_compile v2/patch_library_room_crash.py
python3 v2/patch_library_room_crash.py work/source
python3 -m py_compile v2/patch_throw_null_body.py
python3 v2/patch_throw_null_body.py work/source
python3 -m py_compile v2/patch_orb_enemy_effects.py
python3 v2/patch_orb_enemy_effects.py work/source
python3 -m py_compile v2/patch_worm_camera_orb_audio.py
python3 v2/patch_worm_camera_orb_audio.py work/source
python3 -m py_compile v2/patch_hit_object_rehaunted.py
python3 v2/patch_hit_object_rehaunted.py work/source
python3 -m py_compile v2/patch_cellar_revolver.py
python3 v2/patch_cellar_revolver.py work/source
python3 -m py_compile v2/patch_cellar_evalvar.py
python3 v2/patch_cellar_evalvar.py work/source
python3 -m py_compile v2/patch_rehaunted_mask_reptile.py
python3 v2/patch_rehaunted_mask_reptile.py work/source
python3 -m py_compile v2/patch_cellar_barrel_stuck.py
python3 v2/patch_cellar_barrel_stuck.py work/source
python3 -m py_compile v2/patch_cellar_wall_recovery.py v2/gen_readable_font.py v2/patch_readable_font.py
python3 v2/patch_cellar_wall_recovery.py work/source
python3 v2/gen_readable_font.py work/source
python3 v2/patch_readable_font.py work/source
python3 -m py_compile v2/patch_graphics_regression.py
python3 v2/patch_graphics_regression.py work/source
python3 -m py_compile v2/test_graphics_profile.py
python3 v2/test_graphics_profile.py work/source
python3 -m py_compile v2/patch_startup_splash.py v2/generate_startup_splash.py
python3 v2/patch_startup_splash.py work/source
if [ "$FITD_RELEASE" = "1" ]; then
  # Ubuntu 22.04 ships GCC 11 without <format>; keep glibc 2.35 baseline.
  python3 -m py_compile v2/patch_cxx20_format_portmaster.py
  python3 v2/patch_cxx20_format_portmaster.py work/source
  if grep -Rq 'std::format\|#include <format>' work/source/FitdLib/room.cpp work/source/FitdLib/floor.cpp work/source/FitdLib/config.h; then
    echo "ERROR: remaining unsupported C++20 <format> in FITD engine" >&2
    exit 5
  fi
  python3 -m py_compile v2/prepare_release_arm64.py
  python3 v2/prepare_release_arm64.py work/source
  grep -Fq 'R36S_RELEASE_TIMEBASE' work/source/FitdLib/osystemSDL.cpp
  grep -Fq 'gFrameLimit = 25;' work/source/FitdLib/bgfxGlue.cpp
fi
grep -Fq 'r36sShowStartupNotices();' work/source/FitdLib/osystemSDL.cpp
grep -Fq '[FITD-SPLASH] SHOW slide=' work/source/FitdLib/osystemSDL.cpp
grep -Fq '[FITD-GFX] MASK_DEPTH=' work/source/FitdLib/rendererBGFX.cpp
grep -Fq '[FITD-GFX] NEAR_PRIMITIVE_REJECT' work/source/FitdLib/renderer.cpp
grep -Fq '[FITD-BARRELS] RECOVER_WALL' work/source/FitdLib/main.cpp
grep -Fq '[FITD-FONT] mode=' work/source/FitdLib/font.cpp
grep -Fq 'g_r36sReadable14' work/source/FitdLib/r36sReadableFontData.h
grep -Fq '[FITD-BARRELS] PLAYER' work/source/FitdLib/main.cpp
grep -Fq 'BGFX_STATE_DEPTH_TEST_ALWAYS' work/source/FitdLib/rendererBGFX.cpp
grep -Fq '[FITD-REPTILE] WORLD228' work/source/FitdLib/main.cpp
# Verify generated C++ log format strings before the expensive CMake build.
# Python triple-quoted patches must emit a literal \n, not a raw line break.
python3 - <<'PY'
from pathlib import Path
for filename in ("main.cpp", "life.cpp", "evalVar.cpp"):
    p = Path("work/source/FitdLib") / filename
    for lineno, line in enumerate(p.read_text().splitlines(), 1):
        if '"[FITD-CELLAR]' in line and r'\n"' not in line:
            raise SystemExit(
                f"BAD generated C++ diagnostic string: {p}:{lineno}: {line.strip()}"
            )
print("FITD-CELLAR: C++ diagnostic string escape preflight passed")
PY
# CI must never publish a test binary without the crash and door-audio patches.
grep -Fq 'SAVE COMPLETE slot=' work/source/FitdLib/save.cpp
grep -Fq 'ListWorldObjets.resize(300)' work/source/FitdLib/save.cpp
grep -Fq 'DOOR SAMPLE SUPPRESSED' work/source/FitdLib/tatou.cpp
grep -Fq '[FITD-LIBRARY] INVALID_ROOM' work/source/FitdLib/main.cpp
grep -Fq '[FITD-THROW] MISSING_BODY' work/source/FitdLib/animAction.cpp
grep -Fq '[FITD-ORB] HIT_CHECK' work/source/FitdLib/animAction.cpp
grep -Fq '[FITD-ORB] CONTACT' work/source/FitdLib/main.cpp
grep -Fq '[FITD-EFFECT] SHAKE' work/source/FitdLib/life.cpp
grep -Fq '[FITD-ORB] REP_PLAY' work/source/FitdLib/life.cpp
grep -Fq '[FITD-WORM-CAM] TRACE' work/source/FitdLib/main.cpp
grep -Fq '[FITD-HITOBJ] CONTACT_DAMAGE' work/source/FitdLib/animAction.cpp
grep -Fq '[FITD-CELLAR] FOUND_ACTION' work/source/FitdLib/main.cpp
grep -Fq '[FITD-CELLAR] INVALID_DELETE' work/source/FitdLib/life.cpp
grep -Fq '[FITD-CELLAR] EVAL_BAD_OBJECT' work/source/FitdLib/evalVar.cpp
echo 'FITD R36S: verified save/audio fixes and library invalid-room diagnostics'
cmake -S work/source -B work/arm64 -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_FLAGS="-DFITD_R36S=1 -DBGFX_CONFIG_MULTITHREADED=0 -DBGFX_CONFIG_RENDERER_OPENGL=0 -DBGFX_CONFIG_RENDERER_OPENGLES=30" \
    -DCMAKE_CXX_FLAGS="-DFITD_R36S=1 -DBGFX_CONFIG_MULTITHREADED=0 -DBGFX_CONFIG_RENDERER_OPENGL=0 -DBGFX_CONFIG_RENDERER_OPENGLES=30" \
    -DCMAKE_C_FLAGS_RELEASE="$FITD_BUILD_FLAGS" \
    -DCMAKE_CXX_FLAGS_RELEASE="$FITD_BUILD_FLAGS" \
    -DCMAKE_EXE_LINKER_FLAGS="$FITD_LINK_FLAGS" \
    -DSDL_KMSDRM=ON \
    -DSDL_SHARED=OFF -DSDL_STATIC=ON
cmake --build work/arm64 --target Fitd --parallel 2
BIN="$(find work/arm64 -type f -name Fitd -perm /111 | head -n1)"
test -n "$BIN"
file "$BIN"
readelf -h "$BIN" | grep -q AArch64
PORT="$ROOT/work/package"
mkdir -p "$PORT/fitdr36s/data" "$PORT/fitdr36s/libs.aarch64" "$PORT/fitdr36s/splash" "$PORT/fitdr36s/licenses"
cp "$BIN" "$PORT/fitdr36s/fitd.aarch64"
if [ "$FITD_RELEASE" = "1" ]; then
  strip --strip-unneeded "$PORT/fitdr36s/fitd.aarch64"
  if readelf -SW "$PORT/fitdr36s/fitd.aarch64" | grep -Eq '\.debug_'; then
    echo "ERROR: release binary contains debug sections" >&2
    exit 3
  fi
  cp v2/port/FITD-R36S-ARM64-RELEASE.sh "$PORT/FITD-R36S-ARM64.sh"
  cp v2/port/port-release.json "$PORT/fitdr36s/port.json"
else
  cp v2/port/FITD-R36S-ARM64.sh "$PORT/FITD-R36S-ARM64.sh"
  cp v2/port/port.json "$PORT/fitdr36s/port.json"
fi
cp v2/port/font.conf "$PORT/fitdr36s/font.conf"
cp v2/port/graphics.conf "$PORT/fitdr36s/graphics.conf"
cp v2/port/splash.conf "$PORT/fitdr36s/splash.conf"
cp v2/port/check_compatibility.sh "$PORT/fitdr36s/check_compatibility.sh"
chmod 755 "$PORT/fitdr36s/check_compatibility.sh"
cp v2/port/LEIA-ME-LEGAL.txt "$PORT/fitdr36s/LEIA-ME-LEGAL.txt"
python3 v2/generate_startup_splash.py "$PORT/fitdr36s/splash"
test -s "$PORT/fitdr36s/splash/credits.bmp"
test -s "$PORT/fitdr36s/splash/legal.bmp"
test -s "$PORT/fitdr36s/LEIA-ME-LEGAL.txt"
cp work/source/LICENSE "$PORT/fitdr36s/LICENSE.FITD.txt"
cp work/source/LICENSE "$PORT/fitdr36s/licenses/LICENSE.FITD.txt"
chmod 755 "$PORT/FITD-R36S-ARM64.sh" "$PORT/fitdr36s/fitd.aarch64"
printf 'Copy original legally acquired AITD game files here (not redistributed).\n' > "$PORT/fitdr36s/data/COPY_GAME_FILES_HERE.txt"
{
  echo "Build runner:"; uname -m; getconf GNU_LIBC_VERSION
  echo "Dynamic dependencies:"; ldd "$BIN" || true
  echo "Required GLIBC versions:"; readelf -V "$BIN" | grep -o 'GLIBC_[0-9.]*' | sort -Vu || true
  echo "Required GLIBCXX versions:"; readelf -V "$BIN" | grep -o 'GLIBCXX_[0-9.]*' | sort -Vu || true
} > "$PORT/fitdr36s/build-diagnostics.txt"
if [ "$FITD_RELEASE" = "1" ]; then
  mv "$PORT/fitdr36s/build-diagnostics.txt" "$PORT/fitdr36s/build-info.txt"
fi
# Leave glibc, the dynamic loader and GPU/Mesa/GBM/EGL stack to the firmware.
for lib in libstdc++.so.6 libgcc_s.so.1 libatomic.so.1; do
  source_lib="$(ldconfig -p | awk -v name="$lib" '$1 == name && /aarch64/ {print $NF; exit}')"
  if [ -f "$source_lib" ]; then cp -L "$source_lib" "$PORT/fitdr36s/libs.aarch64/$lib"; fi
done
# Preserve the licenses/copyright notices for any bundled runtime libraries.
for pkg in libstdc++6 libgcc-s1 libatomic1; do
  copyright="/usr/share/doc/$pkg/copyright"
  if [ -f "$copyright" ]; then
    cp "$copyright" "$PORT/fitdr36s/licenses/$pkg-copyright.txt"
  fi
done
# Include third-party source notices where the upstream tree provides them.
find work/source/ThirdParty -maxdepth 5 -type f \
  \( -iname 'LICENSE' -o -iname 'LICENSE.txt' -o -iname 'COPYING' \) \
  -print 2>/dev/null | sort > "$PORT/fitdr36s/licenses/THIRD_PARTY_LICENSE_PATHS.txt"
# Copy third-party license texts from the compiled source tree (not paths only).
python3 - "$PORT/fitdr36s/licenses" <<'PY'
from pathlib import Path
import shutil
import sys
dest = Path(sys.argv[1])
sources = Path("work/source/ThirdParty")
copied = 0
if sources.is_dir():
    for source in sorted(sources.rglob("*")):
        if not source.is_file() or source.stat().st_size > 1_000_000:
            continue
        if source.name.lower() not in ("license", "license.txt", "license.md", "copying", "copying.txt", "copyright"):
            continue
        name = "THIRDPARTY_" + "_".join(source.relative_to(sources).parts)
        shutil.copy2(source, dest / name)
        copied += 1
print(f"Bundled third-party license texts: {copied}")
PY
{
  echo "FITD upstream: https://github.com/yaz0r/FITD"
  echo "Reference engine: https://github.com/jmimu/FITD"
  echo "Port: https://github.com/dehalbuq/FITD-R36S"
  echo "Bundled shared libraries:"
  find "$PORT/fitdr36s/libs.aarch64" -maxdepth 1 -type f -printf '%f\n' | sort
} > "$PORT/fitdr36s/licenses/SOURCES-AND-LIBRARIES.txt"
if [ "$FITD_RELEASE" = "1" ]; then
  # Prevent publishing binaries that require newer glibc than the chosen baseline.
  python3 - "$PORT/fitdr36s/fitd.aarch64" <<'PY'
import re, subprocess, sys
binary = sys.argv[1]
data = subprocess.check_output(['readelf', '-V', binary], text=True)
versions = [tuple(map(int, v.split('.'))) for v in re.findall(r'GLIBC_(\d+\.\d+)', data)]
if versions and max(versions) > (2, 35):
    raise SystemExit(f"Release requires GLIBC_{'.'.join(map(str, max(versions)))}, newer than Ubuntu 22.04 baseline")
print('GLIBC compatibility audit passed: <=2.35')
PY
  python3 v2/package_portmaster_release.py "$ROOT" "$PORT"
  python3 - "$PORT" <<'PY'
from pathlib import Path
import json, sys, xml.etree.ElementTree as ET
p=Path(sys.argv[1])
m=json.loads((p/'port.json').read_text())
assert m['items'] == ['FITD-R36S-ARM64.sh', 'fitdr36s']
assert m['attr']['arch'] == ['aarch64']
assert (p/'fitdr36s/fitd.aarch64').stat().st_size > 0
# Real gameplay images are mandatory for submission but not for local test builds.
if (p/'screenshot.png').exists():
    assert (p/'screenshot.png').stat().st_size > 0
else:
    assert (p/'SUBMISSION_BLOCKERS.txt').is_file()
g=ET.parse(p/'gameinfo.xml').getroot().find('game')
assert g.findtext('path') == './FITD-R36S-ARM64.sh'
assert (p/'fitdr36s/licenses/LICENSE.FITD.txt').is_file()
assert (p/'fitdr36s/check_compatibility.sh').is_file()
print('PortMaster staging QA OK: binary, XML, JSON, launcher, licenses')
PY
fi
(
  cd "$PORT"
  zip -qr "$ROOT/work/$FITD_ARCHIVE" .
)
echo "PORT READY: $ROOT/work/$FITD_ARCHIVE"

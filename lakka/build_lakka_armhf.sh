#!/usr/bin/env bash
# Experimental real ARMHF build from the pinned R36S v1.0.0 gameplay patches.
set -Eeuo pipefail
cd "$(dirname "$0")/.."
[ "$(getconf GNU_LIBC_VERSION)" = "glibc 2.31" ] || { echo "Expected Debian Bullseye glibc 2.31" >&2; exit 2; }
command -v clang++-11 >/dev/null
export CC=clang-11 CXX=clang++-11
mkdir -p work-lakka
rm -rf work-lakka/source work-lakka/build work-lakka/stage
git clone --recursive https://github.com/yaz0r/FITD.git work-lakka/source
git -C work-lakka/source checkout --detach 543e22bc78b64405537ae545ecd382dcdf2c12b2
git -C work-lakka/source submodule update --init --recursive
SRC=work-lakka/source
for patch in patch_upstream.py patch_debug_audio_crash.py patch_library_room_crash.py patch_throw_null_body.py patch_orb_enemy_effects.py patch_worm_camera_orb_audio.py patch_hit_object_rehaunted.py patch_cellar_revolver.py patch_cellar_evalvar.py patch_rehaunted_mask_reptile.py patch_cellar_barrel_stuck.py patch_cellar_wall_recovery.py; do
    python3 "v2/$patch" "$SRC"
done
python3 lakka/gen_font_buster.py "$SRC"
python3 v2/patch_readable_font.py "$SRC"
python3 v2/patch_graphics_regression.py "$SRC"
python3 v2/test_graphics_profile.py "$SRC"
python3 v2/patch_startup_splash.py "$SRC"
python3 v2/patch_cxx20_format_portmaster.py "$SRC"
python3 v2/prepare_release_arm64.py "$SRC"
python3 lakka/patch_lakka_gles2.py "$SRC"
OPTS='-O2 -g0 -DNDEBUG -DFITD_R36S=1 -DFITD_R36S_RELEASE=1 -march=armv7-a -mfpu=neon -mfloat-abi=hard -fno-strict-aliasing -fwrapv -ffunction-sections -fdata-sections'
cmake -S "$SRC" -B work-lakka/build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_FLAGS='-DFITD_R36S=1 -DBGFX_CONFIG_MULTITHREADED=0 -DBGFX_CONFIG_RENDERER_OPENGL=0 -DBGFX_CONFIG_RENDERER_OPENGLES=20' \
  -DCMAKE_CXX_FLAGS='-DFITD_R36S=1 -DBGFX_CONFIG_MULTITHREADED=0 -DBGFX_CONFIG_RENDERER_OPENGL=0 -DBGFX_CONFIG_RENDERER_OPENGLES=20' \
  -DCMAKE_C_FLAGS_RELEASE="$OPTS" -DCMAKE_CXX_FLAGS_RELEASE="$OPTS" \
  -DCMAKE_EXE_LINKER_FLAGS='-Wl,--gc-sections -Wl,--as-needed' \
  -DSDL_KMSDRM=ON -DSDL_SHARED=OFF -DSDL_STATIC=ON
cmake --build work-lakka/build --target Fitd --parallel 2
BIN=$(find work-lakka/build -type f -name Fitd -perm /111 -print -quit)
test -n "$BIN"
file "$BIN" | grep -E 'ELF 32-bit.*ARM'
mkdir -p work-lakka/stage/fitd-lakka/data work-lakka/stage/fitd-lakka/libs.armhf work-lakka/stage/fitd-lakka/licenses
cp "$BIN" work-lakka/stage/fitd-lakka/fitd.armhf
strip --strip-unneeded work-lakka/stage/fitd-lakka/fitd.armhf
! readelf -SW work-lakka/stage/fitd-lakka/fitd.armhf | grep -q '\.debug_'
# The Lakka 4.4 RK322x runtime was reported with glibc 2.32.
python3 - <<'PY'
import re, subprocess
from pathlib import Path
for elf in [Path('work-lakka/stage/fitd-lakka/fitd.armhf'), *Path('work-lakka/stage/fitd-lakka/libs.armhf').glob('*.so*')]:
    out=subprocess.check_output(['readelf','-W','-V',str(elf)],text=True)
    vers=[tuple(map(int,x.split('.'))) for x in re.findall(r'GLIBC_([0-9]+(?:\.[0-9]+)+)',out)]
    if vers and max(vers)>(2,32):
        raise SystemExit('Lakka GLIBC ABI too new for {}: {}'.format(elf,max(vers)))
print('GLIBC ABI compatible with Lakka 2.32')
PY
for lib in libstdc++.so.6 libgcc_s.so.1 libatomic.so.1; do
    path=$(ldconfig -p | awk -v name="$lib" '$1==name {print $NF;exit}')
    [ -f "$path" ] && cp -L "$path" "work-lakka/stage/fitd-lakka/libs.armhf/$lib" || true
done
cp lakka/FITD-LAKKA.sh work-lakka/stage/FITD-LAKKA.sh
cp lakka/diagnostico_lakka.sh work-lakka/stage/fitd-lakka/diagnostico_lakka.sh
cp "$SRC/LICENSE" work-lakka/stage/fitd-lakka/licenses/FITD-GPL.txt
printf 'Copy your legally acquired game files here; no original game data is distributed.\n' > work-lakka/stage/fitd-lakka/data/COPY_GAME_FILES_HERE.txt
chmod +x work-lakka/stage/FITD-LAKKA.sh work-lakka/stage/fitd-lakka/fitd.armhf work-lakka/stage/fitd-lakka/diagnostico_lakka.sh
(cd work-lakka/stage && zip -qyr ../FITD-LAKKA-RK322X-ARMHF-GLES2-NODEBUG.zip .)
(cd work-lakka && sha256sum FITD-LAKKA-RK322X-ARMHF-GLES2-NODEBUG.zip > SHA256SUMS.txt)
unzip -t work-lakka/FITD-LAKKA-RK322X-ARMHF-GLES2-NODEBUG.zip | tail -2
echo "ARMHF GLES2 candidate built (not yet validated on the RK3229 console)."

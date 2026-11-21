#!/usr/bin/env bash
# Native H36S Knulli ARM64+GLES2 build inside Debian Bullseye AArch64 Docker.
set -Eeuo pipefail
cd /workspace
[ "$(uname -m)" = aarch64 ] || { echo "[H36S] ARM64 container required"; exit 2; }
[ "$(getconf GNU_LIBC_VERSION)" = "glibc 2.31" ] || exit 3
export CC=clang-11 CXX=clang++-11
WORK=work-h36s
SRC="$WORK/source"
BUILD="$WORK/build"
STAGE="$WORK/stage"
mkdir -p "$WORK"
if [ "${FITD_H36S_RESUME:-0}" = 1 ]; then
    [ -f "$BUILD/build.ninja" ] || { echo "No cached H36S Ninja build, omit --resume"; exit 4; }
    [ -f "$SRC/FitdLib/osystemSDL.cpp" ] || exit 5
    echo "[H36S] Resuming compiled objects"
else
    FITD_SHA=543e22bc78b64405537ae545ecd382dcdf2c12b2
    HANDKIT_SHA=75d0e6825005f86356afb581b178b69d17173098
    if [ ! -d "$SRC/.git" ]; then
        git clone --recursive https://github.com/yaz0r/FITD.git "$SRC"
    fi
    git -C "$SRC" checkout --detach "$FITD_SHA"
    git -C "$SRC" reset --hard "$FITD_SHA"
    git -C "$SRC" clean -fdx
    git -C "$SRC" submodule update --init --recursive
    if [ ! -d "$WORK/handkit/.git" ]; then
        git clone https://github.com/code-beans/handkit.git "$WORK/handkit"
    fi
    git -C "$WORK/handkit" checkout --detach "$HANDKIT_SHA"

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
    python3 lakka/patch_hqr_reset_body.py "$SRC"
    python3 lakka/patch_lakka_fps.py "$SRC"
    python3 lakka/patch_lakka_audio.py "$SRC"
    python3 lakka/patch_lakka_sdl3_backend.py "$SRC"
    python3 lakka/patch_lakka_perf.py "$SRC"
    python3 lakka/patch_lakka_gles2.py "$SRC"
    python3 knulli/patch_h36s_platform.py "$SRC" "$WORK/handkit"

    OPTS='-O2 -g0 -DNDEBUG -march=armv8-a -mtune=cortex-a53 -fno-strict-aliasing -fwrapv -ffunction-sections -fdata-sections'
    cmake -S "$SRC" -B "$BUILD" -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_C_FLAGS='-DFITD_R36S=1 -DBGFX_CONFIG_MULTITHREADED=0 -DBGFX_CONFIG_RENDERER_OPENGL=0 -DBGFX_CONFIG_RENDERER_OPENGLES=20' \
      -DCMAKE_CXX_FLAGS='-DFITD_R36S=1 -DBGFX_CONFIG_MULTITHREADED=0 -DBGFX_CONFIG_RENDERER_OPENGL=0 -DBGFX_CONFIG_RENDERER_OPENGLES=20' \
      -DCMAKE_C_FLAGS_RELEASE="$OPTS" -DCMAKE_CXX_FLAGS_RELEASE="$OPTS" \
      -DCMAKE_EXE_LINKER_FLAGS='-Wl,--gc-sections -Wl,--as-needed' \
      -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
      -DSDL_SHARED=OFF -DSDL_STATIC=ON \
      -DSDL_X11=OFF -DSDL_WAYLAND=OFF -DSDL_KMSDRM=OFF \
      -DSDL_OPENGL=OFF -DSDL_OPENGLES=ON -DSDL_ALSA=ON -DSDL_LIBUDEV=OFF
fi
cmake --build "$BUILD" --target Fitd --parallel "${FITD_BUILD_JOBS:-2}"
BIN="$(find "$BUILD" -type f -name Fitd -perm /111 -print -quit)"
[ -n "$BIN" ] || { echo "[H36S] Executable missing"; exit 6; }
file "$BIN" | grep -E 'ELF 64-bit.*ARM aarch64|ELF 64-bit.*ARM64'
mkdir -p "$STAGE/fitd-h36s/data" "$STAGE/fitd-h36s/libs.aarch64" "$STAGE/fitd-h36s/licenses"
cp "$SRC/LICENSE" "$STAGE/fitd-h36s/licenses/FITD-GPL.txt"
cp "$WORK/handkit/LICENSE" "$STAGE/fitd-h36s/licenses/HANDKIT-MIT.txt"
cp "$BIN" "$STAGE/fitd-h36s/fitd.aarch64"
strip --strip-unneeded "$STAGE/fitd-h36s/fitd.aarch64"
! readelf -SW "$STAGE/fitd-h36s/fitd.aarch64" | grep -q '\.debug_'
cp knulli/FITD-H36S.sh "$STAGE/FITD-H36S.sh"
chmod 755 "$STAGE/FITD-H36S.sh" "$STAGE/fitd-h36s/fitd.aarch64"
for lib in libstdc++.so.6 libgcc_s.so.1 libatomic.so.1; do
    path="$(ldconfig -p | awk -v name="$lib" '$1==name {print $NF;exit}')"
    [ -n "$path" ] && [ -f "$path" ] && cp -L "$path" "$STAGE/fitd-h36s/libs.aarch64/$lib" || true
done
printf 'Copy your legally acquired original FITD data here.\n' > "$STAGE/fitd-h36s/data/README.txt"
python3 - <<'PY'
import re,subprocess
from pathlib import Path
for f in [Path('work-h36s/stage/fitd-h36s/fitd.aarch64'), *Path('work-h36s/stage/fitd-h36s/libs.aarch64').glob('*.so*')]:
    o=subprocess.check_output(['readelf','-W','-V',str(f)],text=True)
    v=[tuple(map(int,x.split('.'))) for x in re.findall(r'GLIBC_([0-9]+(?:\.[0-9]+)+)',o)]
    if v and max(v)>(2,33): raise SystemExit(f'ABI too new: {f}: {max(v)}')
print('[H36S] ELF/GLIBC ABI checked')
PY
(cd "$STAGE" && zip -qyr ../FITD-H36S-KNULLI-ARM64-GLES2-TEST.zip .)
unzip -t "$WORK/FITD-H36S-KNULLI-ARM64-GLES2-TEST.zip" | tail -2
sha256sum "$WORK/FITD-H36S-KNULLI-ARM64-GLES2-TEST.zip" > "$WORK/SHA256SUMS.txt"
echo "[H36S] Candidate package: $WORK/FITD-H36S-KNULLI-ARM64-GLES2-TEST.zip"

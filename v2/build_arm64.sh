#!/usr/bin/env bash
set -Eeuo pipefail
cd "$(dirname "$0")/.."
ROOT="$PWD"
UPSTREAM_SHA="543e22bc78b64405537ae545ecd382dcdf2c12b2"
mkdir -p work
rm -rf work/source work/arm64 work/package
git clone --recursive https://github.com/yaz0r/FITD.git work/source
git -C work/source checkout --detach "$UPSTREAM_SHA"
git -C work/source submodule update --init --recursive
python3 v2/patch_upstream.py work/source
cmake -S work/source -B work/arm64 -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_FLAGS="-DFITD_R36S=1 -DBGFX_CONFIG_MULTITHREADED=0 -DBGFX_CONFIG_RENDERER_OPENGL=0 -DBGFX_CONFIG_RENDERER_OPENGLES=30" \
    -DCMAKE_CXX_FLAGS="-DFITD_R36S=1 -DBGFX_CONFIG_MULTITHREADED=0 -DBGFX_CONFIG_RENDERER_OPENGL=0 -DBGFX_CONFIG_RENDERER_OPENGLES=30" \
    -DCMAKE_C_FLAGS_RELEASE="-O2 -g1 -fno-omit-frame-pointer -DNDEBUG -march=armv8-a" \
    -DCMAKE_CXX_FLAGS_RELEASE="-O2 -g1 -fno-omit-frame-pointer -DNDEBUG -march=armv8-a" \
    -DSDL_KMSDRM=ON \
    -DSDL_SHARED=OFF -DSDL_STATIC=ON
cmake --build work/arm64 --target Fitd --parallel 2
BIN="$(find work/arm64 -type f -name Fitd -perm /111 | head -n1)"
test -n "$BIN"
file "$BIN"
readelf -h "$BIN" | grep -q AArch64
PORT="$ROOT/work/package"
mkdir -p "$PORT/fitdr36s/data" "$PORT/fitdr36s/libs"
cp "$BIN" "$PORT/fitdr36s/fitd.aarch64"
cp v2/port/FITD-R36S-ARM64.sh "$PORT/FITD-R36S-ARM64.sh"
cp v2/port/port.json "$PORT/fitdr36s/port.json"
cp work/source/LICENSE "$PORT/fitdr36s/LICENSE.FITD.txt"
chmod 755 "$PORT/FITD-R36S-ARM64.sh" "$PORT/fitdr36s/fitd.aarch64"
printf 'Copy original legally acquired AITD game files here (not redistributed).\n' > "$PORT/fitdr36s/data/COPY_GAME_FILES_HERE.txt"
{
  echo "Build runner:"; uname -m; getconf GNU_LIBC_VERSION
  echo "Dynamic dependencies:"; ldd "$BIN" || true
  echo "Required GLIBC versions:"; readelf -V "$BIN" | grep -o 'GLIBC_[0-9.]*' | sort -Vu || true
  echo "Required GLIBCXX versions:"; readelf -V "$BIN" | grep -o 'GLIBCXX_[0-9.]*' | sort -Vu || true
} > "$PORT/fitdr36s/build-diagnostics.txt"
# Leave glibc, the dynamic loader and GPU/Mesa/GBM/EGL stack to the firmware.
for lib in libstdc++.so.6 libgcc_s.so.1 libatomic.so.1; do
  source_lib="$(ldconfig -p | awk -v name="$lib" '$1 == name && /aarch64/ {print $NF; exit}')"
  if [ -f "$source_lib" ]; then cp -L "$source_lib" "$PORT/fitdr36s/libs/$lib"; fi
done
(
  cd "$PORT"
  zip -qr "$ROOT/work/fitdr36s-aarch64-kmsdrm-fix.zip" .
)
echo "PORT READY: $ROOT/work/fitdr36s-aarch64-kmsdrm-fix.zip"

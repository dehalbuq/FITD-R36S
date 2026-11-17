#!/usr/bin/env bash
# Run this INSIDE the Kali self-hosted GitHub runner checkout, NOT on the Lakka box.
# One-time image setup uses QEMU; subsequent builds reuse Docker layers and ccache.
set -Eeuo pipefail
cd "$(dirname "$0")/.."
if [ "$(uname -m)" != "x86_64" ]; then
  echo 'This helper expects x86_64 Kali. Use the original ARMHF workflow on other hosts.' >&2; exit 2
fi
if ! docker info >/dev/null 2>&1; then
  echo 'Docker unavailable to this runner account; enable Docker, add runner to docker group, and restart the runner service.' >&2
  exit 3
fi
if ! docker run --rm --platform linux/arm/v7 debian:bullseye /bin/sh -c 'test "$(uname -m)" = "armv7l"'; then
  echo 'QEMU binfmt ARM not enabled. On Kali: sudo apt install qemu-user-static binfmt-support' >&2
  echo 'Alternatively: sudo docker run --rm --privileged tonistiigi/binfmt --install arm' >&2
  exit 4
fi
CACHE_ROOT="\${FITD_KALI_CACHE:-$HOME/.cache/fitd-lakka}"
mkdir -p "$CACHE_ROOT/docker-context/lakka" "$CACHE_ROOT/ccache"
cp lakka/container_build.sh lakka/gen_font_buster.py "$CACHE_ROOT/docker-context/lakka/"
echo '[1/3] Reusing local toolchain Docker image/layers...'
docker build --platform linux/arm/v7 \
  -f lakka/Dockerfile.kali-armhf \
  -t fitd-lakka-armhf-bullseye:1 \
  "$CACHE_ROOT/docker-context"
JOBS="\${FITD_BUILD_JOBS:-3}"
case "$JOBS" in ''|*[!0-9]*) echo "FITD_BUILD_JOBS must be a number" >&2; exit 5;; esac
if [ "$JOBS" -lt 1 ] || [ "$JOBS" -gt 8 ]; then echo "FITD_BUILD_JOBS must be 1..8" >&2; exit 5; fi
echo "[2/3] ARMHF compiling on THIS Kali CPU, QEMU user emulation, workers=$JOBS"
echo '[CACHE] Reusing upstream sources and ccache after the first build'
docker run --rm --platform linux/arm/v7 \
  --user "$(id -u):$(id -g)" \
  -v "$PWD:/workspace" \
  -v "$CACHE_ROOT/ccache:/ccache" \
  -e HOME=/tmp \
  -e CCACHE_DIR=/ccache \
  -e CCACHE_BASEDIR=/workspace \
  -e CCACHE_COMPILERCHECK=content \
  -e FITD_COMPILER_LAUNCHER=ccache \
  -e FITD_KEEP_SOURCE=1 \
  -e FITD_BUILD_JOBS="$JOBS" \
  -w /workspace \
  fitd-lakka-armhf-bullseye:1 bash lakka/build_lakka_armhf.sh
echo '[3/3] Generated release archive:'
ls -lh work-lakka/FITD-LAKKA-RK322X-ARMHF-GLES2-NODEBUG.zip

#!/usr/bin/env bash
# Run on Kali x86-64. Saves complete stdout/stderr to work-h36s/logs automatically.
set -Eeuo pipefail
cd "$(dirname "$0")/.."
MODE=0
case "${1:-}" in
    "") ;;
    --resume) MODE=1 ;;
    *) echo 'Usage: sudo bash knulli/run_kali_build.sh [--resume]' >&2; exit 2 ;;
esac
mkdir -p work-h36s/logs
LOG="work-h36s/logs/build-h36s-$(date +%Y%m%d-%H%M%S).log"
exec > >(tee "$LOG") 2>&1
trap 'status=$?; echo "[H36S] Exit status=$status; full log=$LOG"' EXIT
echo "[H36S] Build starting; log=$LOG; resume=$MODE"
[ "$(uname -m)" = x86_64 ] || { echo "Use x86_64 Kali host"; exit 3; }
docker info >/dev/null 2>&1 || { echo "Docker is unavailable"; exit 4; }
if ! docker run --rm --platform linux/arm64 debian:bullseye /bin/sh -c 'uname -m'; then
    echo "ARM64 QEMU binfmt unavailable; install qemu-user-static and binfmt-support on Kali"
    exit 5
fi
CACHE="${FITD_KALI_CACHE:-$HOME/.cache/fitd-h36s}"
mkdir -p "$CACHE/docker-context/lakka" "$CACHE/ccache"
IMAGE=fitd-h36s-bullseye-aarch64:1
if [ "$MODE" = 0 ] || ! docker image inspect "$IMAGE" >/dev/null 2>&1; then
    cp lakka/container_build.sh lakka/gen_font_buster.py "$CACHE/docker-context/lakka/"
    docker build --platform linux/arm64 -f knulli/Dockerfile.aarch64 -t "$IMAGE" "$CACHE/docker-context"
fi
JOBS="${FITD_BUILD_JOBS:-2}"
case "$JOBS" in *[!0-9]*|"") echo "Invalid FITD_BUILD_JOBS"; exit 6;; esac
[ "$JOBS" -ge 1 ] && [ "$JOBS" -le 8 ] || { echo "FITD_BUILD_JOBS 1..8"; exit 7; }
docker run --rm --platform linux/arm64 \
    -v "$PWD:/workspace" -v "$CACHE/ccache:/ccache" \
    -e HOME=/tmp \
    -e CCACHE_DIR=/ccache -e CCACHE_BASEDIR=/workspace \
    -e CCACHE_COMPILERCHECK=content \
    -e FITD_H36S_RESUME="$MODE" -e FITD_BUILD_JOBS="$JOBS" \
    -w /workspace "$IMAGE" bash knulli/build_h36s_aarch64.sh
echo "[H36S] Build done"
ls -lh work-h36s/FITD-H36S-KNULLI-ARM64-GLES2-TEST.zip

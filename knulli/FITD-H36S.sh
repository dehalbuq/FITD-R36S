#!/bin/sh
# Knulli H36S Allwinner A133P / PowerVR GE8300; ARM64 + GLES2.
set -u
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
GAME="$ROOT/fitd-h36s"
BIN="$GAME/fitd.aarch64"
DATA="$GAME/data"
LOG="$GAME/fitd-h36s.log"
mkdir -p "$GAME" 2>/dev/null || true
if ! ( : >> "$LOG" ) 2>/dev/null; then LOG="/tmp/fitd-h36s.log"; fi
echo "[FITD-H36S] $(date) arch=$(uname -m) start" >> "$LOG"
case "$(uname -m)" in aarch64|arm64) : ;; *) echo "ERROR expected aarch64" >> "$LOG"; exit 2 ;; esac
[ -x "$BIN" ] || { echo "ERROR: missing executable $BIN" >> "$LOG"; exit 3; }
[ -d "$DATA" ] || { echo "ERROR: data directory missing" >> "$LOG"; exit 4; }
[ -f "$DATA/PERE.PAK" ] || [ -f "$DATA/MER.PAK" ] || [ -f "$DATA/LISTBOD2.PAK" ] || { echo "ERROR: missing original PAK files" >> "$LOG"; exit 5; }
export LD_LIBRARY_PATH="$GAME/libs.aarch64${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export SDL_VIDEODRIVER=handkit
export SDL_AUDIODRIVER="${SDL_AUDIODRIVER:-alsa}"
export SDL_VIDEO_FORCE_EGL=1
export FITD_VSYNC="${FITD_VSYNC:-0}"
export FITD_FPS_LIMIT="${FITD_FPS_LIMIT:-60}"
export FITD_R36S_TIMEBASE="${FITD_R36S_TIMEBASE:-legacy}"
export FITD_DEBUG=0 FITD_VIDEO_TEST=0 FITD_RELEASE_LOG=0
export FITD_LAKKA_HIDE_IMGUI_BAR=1
export FITD_PERF_TRACE="${FITD_PERF_TRACE:-1}"
unset SDL_AUDIO_ALSA_DEFAULT_PLAYBACK_DEVICE ALSA_CONFIG_PATH AUDIODEV
# Do not reuse the RK3229 HDMI ALSA device; Knulli A133P enumerates differently.
cd "$DATA" || exit 7
"$BIN" >> "$LOG" 2>&1
code=$?
echo "[FITD-H36S] exit=$code" >> "$LOG"
exit "$code"

#!/bin/sh
# Standalone Lakka launcher; doesn't modify the base system.
set -u
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
GAME="$ROOT/fitd-lakka"
BIN="$GAME/fitd.armhf"
DATA="$GAME/data"
LOG="$GAME/fitd-lakka.log"
case "$(uname -m)" in armv7*) ;; *) echo "ARMHF build requires ARMv7" >&2; exit 2;; esac
[ -x "$BIN" ] || { echo "Missing: $BIN" >&2; exit 3; }
[ -d "$DATA" ] || { echo "Missing game data directory" >&2; exit 4; }
[ -f "$DATA/LISTBOD2.PAK" ] || [ -f "$DATA/PERE.PAK" ] || [ -f "$DATA/MER.PAK" ] || { echo "Please copy original game .PAK files into $DATA" >&2; exit 5; }
[ -x /lib/ld-linux-armhf.so.3 ] || { echo 'Missing system ARMHF glibc loader: incompatible firmware ABI' >&2; exit 6; }
export LD_LIBRARY_PATH="$GAME/libs.armhf${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export SDL_VIDEODRIVER="${SDL_VIDEODRIVER:-kmsdrm}"
export SDL_AUDIODRIVER="${SDL_AUDIODRIVER:-alsa}"
export SDL_VIDEO_FORCE_EGL=1
export FITD_FPS_LIMIT="${FITD_FPS_LIMIT:-30}"
export FITD_R36S_TIMEBASE="${FITD_R36S_TIMEBASE:-legacy}"
export FITD_VSYNC="${FITD_VSYNC:-0}"
export FITD_DEBUG=0 FITD_VIDEO_TEST=0 FITD_RELEASE_LOG=0
unset FITD_DEEP_TRACE FITD_GDB FITD_AI_TRACE FITD_PERF_TRACE
cd "$DATA" || exit 7
echo "$(date): starting ARMHF GLES2 Release; SDL=$SDL_VIDEODRIVER FPS=$FITD_FPS_LIMIT" >> "$LOG"
"$BIN" >> "$LOG" 2>&1
code=$?
echo "Exit: $code" >> "$LOG"
exit "$code"

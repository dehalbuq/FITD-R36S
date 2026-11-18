#!/bin/sh
# FITD Lakka ARMHF/GLES2 launcher. Never changes the firmware or GPU nodes.
set -u
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
GAME="$ROOT/fitd-lakka"
BIN="$GAME/fitd.armhf"
DATA="$GAME/data"
LOG="$GAME/fitd-lakka.log"

# Preserve an error report even if the game directory is read-only.
if ! ( : >> "$LOG" ) 2>/dev/null; then
    mkdir -p /storage/.cache/fitd-lakka 2>/dev/null || true
    LOG="/storage/.cache/fitd-lakka/fitd-lakka.log"
fi
if ! ( : >> "$LOG" ) 2>/dev/null; then
    LOG="/tmp/fitd-lakka-$(id -u).log"
fi
if ! ( : >> "$LOG" ) 2>/dev/null; then
    echo "FITD: unable to write diagnostics to $LOG" >&2
    exit 73
fi

note() { printf '%s\n' "$*" | tee -a "$LOG" >&2; }
fail() { note "[FITD-LAKKA] ERROR($1): $2"; exit "$1"; }

note "[FITD-LAKKA] $(date) uid=$(id -u) $(uname -m) startup"
note "[FITD-LAKKA] BIN=$BIN DATA=$DATA LOG=$LOG"
case "$(uname -m)" in armv7*|armv6*) :;; *) fail 2 "Expected Linux ARMHF device, got $(uname -m)";; esac
[ -f "$BIN" ] || fail 3 "Executable not found: $BIN"
if [ ! -x "$BIN" ]; then
    ls -ld "$GAME" "$BIN" >> "$LOG" 2>&1 || true
    fail 126 "Executable lacks +x permission, or its filesystem has noexec: $BIN"
fi
[ -d "$DATA" ] || fail 4 "Missing data directory: $DATA"
[ -f "$DATA/LISTBOD2.PAK" ] || [ -f "$DATA/PERE.PAK" ] || [ -f "$DATA/MER.PAK" ] || fail 5 "Original game .PAK files not found in $DATA"

# Loader locations differ between builds; let the kernel report errors rather
# than rejecting valid installs based on a hard-coded /lib path.
if command -v readelf >/dev/null 2>&1; then
    readelf -l "$BIN" 2>/dev/null | grep 'Requesting program interpreter' >> "$LOG" || true
fi
if command -v findmnt >/dev/null 2>&1; then
    findmnt -T "$BIN" -o TARGET,FSTYPE,OPTIONS >> "$LOG" 2>&1 || true
fi

export LD_LIBRARY_PATH="$GAME/libs.armhf${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export SDL_VIDEODRIVER="${SDL_VIDEODRIVER:-kmsdrm}"
export SDL_AUDIODRIVER="${SDL_AUDIODRIVER:-alsa}"
export SDL_VIDEO_FORCE_EGL=1
export FITD_FPS_LIMIT="${FITD_FPS_LIMIT:-60}"
export FITD_R36S_TIMEBASE="${FITD_R36S_TIMEBASE:-legacy}"
export FITD_VSYNC="${FITD_VSYNC:-0}"
export FITD_DEBUG=0 FITD_VIDEO_TEST=0 FITD_RELEASE_LOG=0
unset FITD_DEEP_TRACE FITD_GDB FITD_AI_TRACE FITD_PERF_TRACE
cd "$DATA" || fail 7 "Cannot cd into game data directory $DATA"
note "[FITD-LAKKA] Launching; SDL=$SDL_VIDEODRIVER FPS=$FITD_FPS_LIMIT"
"$BIN" >> "$LOG" 2>&1
code=$?
note "[FITD-LAKKA] Exit: $code; log=$LOG"
exit "$code"

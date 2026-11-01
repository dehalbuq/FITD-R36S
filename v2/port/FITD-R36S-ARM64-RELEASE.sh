#!/bin/bash
# R36S ARM64 safe release: quiet execution without GDB or diagnostic traces.
set -eu
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
GAME="$ROOT/fitdr36s"
BIN="$GAME/fitd.aarch64"
DATA="$GAME/data"

if [ "$(uname -m)" != "aarch64" ]; then
    echo "FITD: requer Linux AArch64" >&2; exit 21
fi
if [ ! -x "$BIN" ]; then
    echo "FITD: executavel ausente: $BIN" >&2; exit 22
fi
if [ ! -d "$DATA" ]; then
    echo "FITD: pasta de dados originais ausente: $DATA" >&2; exit 23
fi
found=0
for pak in LISTBOD2.PAK PERE.PAK MER.PAK AN1.PAK PURSUIT.PAK; do
    if [ -f "$DATA/$pak" ]; then found=1; break; fi
done
if [ "$found" -eq 0 ]; then
    echo "FITD: adicione os PAKs originais em $DATA" >&2; exit 24
fi

# Engine/script clock of the original game: don't accelerate world simulation.
export FITD_FPS_LIMIT=25 FITD_VSYNC=1 FITD_DEBUG=0 FITD_VIDEO_TEST=0
unset FITD_DEEP_TRACE FITD_PERF_TRACE FITD_AI_TRACE FITD_NPC_MOTION_TRACE FITD_SHOT_TRACE FITD_GDB FITD_TEST_TIMEOUT || true
export FITD_WINDOW_RECOVERY="${FITD_WINDOW_RECOVERY:-1}"
export SDL_VIDEODRIVER="${SDL_VIDEODRIVER:-kmsdrm}"
export SDL_AUDIODRIVER="${SDL_AUDIODRIVER:-alsa}"
export SDL_VIDEO_FORCE_EGL=1
export LD_LIBRARY_PATH="$GAME/libs${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

FONT=readable; SIZE=12
if [ -f "$GAME/font.conf" ]; then
    v="$(sed -n 's/^FONT_MODE=//p' "$GAME/font.conf" | head -n1 | tr -d '\r')"
    case "$v" in readable|classic) FONT="$v";; esac
    v="$(sed -n 's/^FONT_SIZE=//p' "$GAME/font.conf" | head -n1 | tr -d '\r')"
    case "$v" in 12|14) SIZE="$v";; esac
fi
export FITD_R36S_FONT="$FONT" FITD_R36S_FONT_SIZE="$SIZE"

MASK=classic; CLIP=off
if [ -f "$GAME/graphics.conf" ]; then
    v="$(sed -n 's/^MASK_DEPTH=//p' "$GAME/graphics.conf" | head -n1 | tr -d '\r')"
    case "$v" in classic|rehaunted) MASK="$v";; esac
    v="$(sed -n 's/^NEAR_CLIP=//p' "$GAME/graphics.conf" | head -n1 | tr -d '\r')"
    case "$v" in on|off) CLIP="$v";; esac
fi
export FITD_R36S_MASK_DEPTH="$MASK" FITD_R36S_NEAR_CLIP="$CLIP"

SPLASH=on; CREDITS=3500; LEGAL=9000
if [ -f "$GAME/splash.conf" ]; then
    v="$(sed -n 's/^SPLASH=//p' "$GAME/splash.conf" | head -n1 | tr -d '\r')"
    case "$v" in on|off) SPLASH="$v";; esac
    v="$(sed -n 's/^CREDITS_MS=//p' "$GAME/splash.conf" | head -n1 | tr -d '\r')"
    case "$v" in 1500|2000|2500|3000|3500|4000|5000|6000|8000|10000|15000) CREDITS="$v";; esac
    v="$(sed -n 's/^LEGAL_MS=//p' "$GAME/splash.conf" | head -n1 | tr -d '\r')"
    case "$v" in 1500|2000|2500|3000|3500|4000|5000|6000|8000|9000|10000|15000|20000) LEGAL="$v";; esac
fi
export FITD_R36S_SPLASH="$SPLASH"
export FITD_R36S_CREDITS_MS="$CREDITS"
export FITD_R36S_LEGAL_MS="$LEGAL"
export FITD_R36S_SPLASH_DIR="$GAME/splash"

cd "$DATA"
if [ "${FITD_RELEASE_LOG:-0}" = "1" ]; then
    # Opt in when investigating a rare fault.
    mkdir -p "$GAME/logs" 2>/dev/null || true
    exec "$BIN" >> "$GAME/logs/fitd-release.log" 2>&1
else
    exec "$BIN" >/dev/null 2>&1
fi

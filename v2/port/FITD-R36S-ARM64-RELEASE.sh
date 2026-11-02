#!/bin/bash
# R36S ARM64 safe release: quiet execution without GDB or diagnostic traces.
set -eu
# PortMaster's control.txt supplies per-firmware device, input and paths.
# A standalone fallback permits manual launch on an AArch64 system.
XDG_DATA_HOME=${XDG_DATA_HOME:-"$HOME/.local/share"}
controlfolder=""
for candidate in /opt/system/Tools/PortMaster /opt/tools/PortMaster "$XDG_DATA_HOME/PortMaster" /roms/ports/PortMaster; do
    if [ -f "$candidate/control.txt" ]; then controlfolder="$candidate"; break; fi
done
if [ -n "$controlfolder" ]; then
    # shellcheck disable=SC1090
    source "$controlfolder/control.txt"
    if [ -n "${CFW_NAME:-}" ] && [ -f "$controlfolder/mod_${CFW_NAME}.txt" ]; then
        # shellcheck disable=SC1090
        source "$controlfolder/mod_${CFW_NAME}.txt"
    fi
    if declare -F get_controls >/dev/null 2>&1; then get_controls; fi
fi
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
GAME="$ROOT/fitdr36s"
BIN="$GAME/fitd.aarch64"
DATA="$GAME/data"
DEVICE_ARCH=${DEVICE_ARCH:-$(uname -m)}
case "$DEVICE_ARCH" in aarch64|arm64) ;; *)
  echo "FITD: this build only supports AArch64 (found $DEVICE_ARCH)." >&2; exit 21 ;;
esac

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

# Match the previously playable debug build's per-frame simulation.
# The experimental real-time 25Hz scheduler slowed character movement.
# Opt in to FITD_R36S_TIMEBASE=real only for explicit A/B testing.
# The legacy mode can accelerate some scripts at higher render FPS:
# the game still needs a full render/simulation decoupling fix.
export FITD_FPS_LIMIT="${FITD_FPS_LIMIT:-60}"
export FITD_R36S_TIMEBASE="${FITD_R36S_TIMEBASE:-legacy}"
export FITD_VSYNC="${FITD_VSYNC:-0}" FITD_DEBUG=0 FITD_VIDEO_TEST=0
unset FITD_DEEP_TRACE FITD_PERF_TRACE FITD_AI_TRACE FITD_NPC_MOTION_TRACE FITD_SHOT_TRACE FITD_GDB FITD_TEST_TIMEOUT || true
export FITD_WINDOW_RECOVERY="${FITD_WINDOW_RECOVERY:-1}"
# SDL3/KMSDRM works on the tested RK3326; do not override firmware settings.
export SDL_VIDEODRIVER="${SDL_VIDEODRIVER:-kmsdrm}"
[ -z "${sdl_controllerconfig:-}" ] || export SDL_GAMECONTROLLERCONFIG="$sdl_controllerconfig"
[ -z "${sdl_controllerconfig:-}" ] || export SDL_GAMECONTROLLERCONFIG_FILE="$sdl_controllerconfig"
export SDL_AUDIODRIVER="${SDL_AUDIODRIVER:-alsa}"
export SDL_VIDEO_FORCE_EGL=1
# Never bundle/override firmware GPU/EGL/DRM, dynamic loader or glibc.
if [ -d "$GAME/libs.aarch64" ]; then
    export LD_LIBRARY_PATH="$GAME/libs.aarch64${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi

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

# PortMaster adapts SDL/video and input to the detected firmware.
if declare -F pm_platform_helper >/dev/null 2>&1; then
    pm_platform_helper "$BIN"
fi
# The game uses native SDL3 gamepad handling; gptokeyb supplies the standard
# PortMaster emergency exit shortcut where provided.
GPTK_PID=""
if [ -n "${GPTOKEYB:-}" ]; then
    # The PortMaster environment normally defines GPTOKEYB as a command path.
    $GPTOKEYB "$(basename "$BIN")" &
    GPTK_PID=$!
fi
finish() {
    if [ -n "$GPTK_PID" ]; then kill "$GPTK_PID" 2>/dev/null || true; fi
    if declare -F pm_finish >/dev/null 2>&1; then pm_finish; fi
}
trap finish EXIT
cd "$DATA"
if [ "${FITD_RELEASE_LOG:-0}" = "1" ]; then
    mkdir -p "$GAME/logs" 2>/dev/null || true
    "$BIN" >> "$GAME/logs/fitd-release.log" 2>&1
else
    "$BIN" >/dev/null 2>&1
fi

#!/bin/bash
# FITD for PortMaster: ARM64 GLES3 experimental. Original game files required.
# Keep this script POSIX-friendly where practical; control.txt needs bash.
set -e

XDG_DATA_HOME=${XDG_DATA_HOME:-"$HOME/.local/share"}
if [ -f /opt/system/Tools/PortMaster/control.txt ]; then
    controlfolder=/opt/system/Tools/PortMaster
elif [ -f /opt/tools/PortMaster/control.txt ]; then
    controlfolder=/opt/tools/PortMaster
elif [ -f "$XDG_DATA_HOME/PortMaster/control.txt" ]; then
    controlfolder="$XDG_DATA_HOME/PortMaster"
else
    controlfolder=/roms/ports/PortMaster
fi

# A missing control.txt means PortMaster is not correctly installed.
# Preserve a standalone test path for developers.
if [ -f "$controlfolder/control.txt" ]; then
    # shellcheck source=/dev/null
    source "$controlfolder/control.txt"
    if [ -n "${CFW_NAME:-}" ] && [ -f "$controlfolder/mod_${CFW_NAME}.txt" ]; then
        # shellcheck source=/dev/null
        source "$controlfolder/mod_${CFW_NAME}.txt"
    fi
    if declare -F get_controls >/dev/null 2>&1; then get_controls; fi
fi

PORTDIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
GAMEDIR="$PORTDIR/fitdr36s"
BIN="$GAMEDIR/fitd.aarch64"
LIBDIR="$GAMEDIR/libs.aarch64"
DATA="$GAMEDIR/data"
# Keep the original dArkOSRE executable as the default. ArkOS4Clone's
# glibc 2.30 requires a separately ABI-audited binary and library set.
# Select the legacy variant only on an older glibc; never replace the
# known dArkOSRE executable or override the firmware's GPU/GLIBC libraries.
GLIBC_TEXT="$(getconf GNU_LIBC_VERSION 2>/dev/null || true)"
GLIBC_MINOR="$(printf '%s\n' "$GLIBC_TEXT" | sed -n 's/^glibc 2\\.\\([0-9]*\\).*$/\\1/p')"
if [ -n "$GLIBC_MINOR" ] && [ "$GLIBC_MINOR" -le 30 ]; then
    BIN="$GAMEDIR/fitd.glibc230.aarch64"
    LIBDIR="$GAMEDIR/libs.glibc230"
    if [ ! -x "$BIN" ]; then
        mkdir -p "$GAMEDIR/logs" || true
        echo "FITD: firmware uses $GLIBC_TEXT; needs ABI-audited glibc<=2.30 ARM64 executable." | tee "$GAMEDIR/logs/compatibility-error.txt" >&2
        echo "FITD: the dArkOSRE binary is intentionally preserved, not replaced." >&2
        exit 25
    fi
fi
ARCH=${DEVICE_ARCH:-$(uname -m)}
case "$ARCH" in
    aarch64|arm64) ;;
    *) echo "FITD: this package requires AArch64; got $ARCH" >&2; exit 21 ;;
esac

if [ ! -x "$BIN" ]; then
    echo "FITD: missing executable $BIN" >&2
    exit 22
fi
if [ ! -d "$DATA" ]; then
    echo "FITD: missing original game data directory $DATA" >&2
    exit 23
fi
found=0
for pak in LISTBOD2.PAK PERE.PAK MER.PAK AN1.PAK PURSUIT.PAK; do
    if [ -f "$DATA/$pak" ]; then found=1; break; fi
done
if [ "$found" -eq 0 ]; then
    echo "FITD: original game .PAK files required in $DATA" >&2
    if declare -F pm_message >/dev/null 2>&1; then
        pm_message "Please copy your original Alone in the Dark game files to fitdr36s/data/"
    fi
    exit 24
fi

# Select platform-specific dependencies. Do not bundle glibc/EGL/DRM/GPU stack.
if [ -d "$LIBDIR" ]; then
    export LD_LIBRARY_PATH="$LIBDIR${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi
if [ -n "${sdl_controllerconfig:-}" ]; then
    export SDL_GAMECONTROLLERCONFIG="$sdl_controllerconfig"
fi

# SDL3 has its own video backend; KMSDRM works on the tested dArkOSRE path.
# The firmware or user can override the driver to another SDL3 backend.
export SDL_VIDEODRIVER="${SDL_VIDEODRIVER:-kmsdrm}"
export SDL_AUDIODRIVER="${SDL_AUDIODRIVER:-alsa}"
export SDL_VIDEO_FORCE_EGL=1

# The Debug build was playable; avoid experimental 25Hz release-only slowdown.
export FITD_FPS_LIMIT="${FITD_FPS_LIMIT:-60}"
export FITD_R36S_TIMEBASE="${FITD_R36S_TIMEBASE:-legacy}"
export FITD_VSYNC="${FITD_VSYNC:-0}"
export FITD_DEBUG=0 FITD_VIDEO_TEST=0
unset FITD_DEEP_TRACE FITD_PERF_TRACE FITD_AI_TRACE FITD_NPC_MOTION_TRACE
unset FITD_SHOT_TRACE FITD_GDB FITD_TEST_TIMEOUT
export FITD_WINDOW_RECOVERY="${FITD_WINDOW_RECOVERY:-1}"

FONT=readable
SIZE=12
if [ -f "$GAMEDIR/font.conf" ]; then
    v="$(sed -n 's/^FONT_MODE=//p' "$GAMEDIR/font.conf" | head -n1 | tr -d '\r')"
    case "$v" in readable|classic) FONT="$v" ;; esac
    v="$(sed -n 's/^FONT_SIZE=//p' "$GAMEDIR/font.conf" | head -n1 | tr -d '\r')"
    case "$v" in 12|14) SIZE="$v" ;; esac
fi
export FITD_R36S_FONT="$FONT" FITD_R36S_FONT_SIZE="$SIZE"

MASK=classic
CLIP=off
if [ -f "$GAMEDIR/graphics.conf" ]; then
    v="$(sed -n 's/^MASK_DEPTH=//p' "$GAMEDIR/graphics.conf" | head -n1 | tr -d '\r')"
    case "$v" in classic|rehaunted) MASK="$v" ;; esac
    v="$(sed -n 's/^NEAR_CLIP=//p' "$GAMEDIR/graphics.conf" | head -n1 | tr -d '\r')"
    case "$v" in on|off) CLIP="$v" ;; esac
fi
export FITD_R36S_MASK_DEPTH="$MASK" FITD_R36S_NEAR_CLIP="$CLIP"

SPLASH=on
CREDITS=3500
LEGAL=9000
if [ -f "$GAMEDIR/splash.conf" ]; then
    v="$(sed -n 's/^SPLASH=//p' "$GAMEDIR/splash.conf" | head -n1 | tr -d '\r')"
    case "$v" in on|off) SPLASH="$v" ;; esac
    v="$(sed -n 's/^CREDITS_MS=//p' "$GAMEDIR/splash.conf" | head -n1 | tr -d '\r')"
    case "$v" in 1500|2000|2500|3000|3500|4000|5000|6000|8000|10000|15000) CREDITS="$v" ;; esac
    v="$(sed -n 's/^LEGAL_MS=//p' "$GAMEDIR/splash.conf" | head -n1 | tr -d '\r')"
    case "$v" in 1500|2000|2500|3000|3500|4000|5000|6000|8000|9000|10000|15000|20000) LEGAL="$v" ;; esac
fi
export FITD_R36S_SPLASH="$SPLASH"
export FITD_R36S_CREDITS_MS="$CREDITS"
export FITD_R36S_LEGAL_MS="$LEGAL"
export FITD_R36S_SPLASH_DIR="$GAMEDIR/splash"

GPTK_PID=""
finish() {
    if [ -n "$GPTK_PID" ]; then
        kill "$GPTK_PID" 2>/dev/null || true
        wait "$GPTK_PID" 2>/dev/null || true
    fi
    if declare -F pm_finish >/dev/null 2>&1; then pm_finish; fi
}
trap finish EXIT

if [ -n "${GPTOKEYB:-}" ]; then
    # Standard PortMaster device-specific emergency exit; SDL3 handles controls.
    "$GPTOKEYB" "$(basename "$BIN")" &
    GPTK_PID=$!
fi
if declare -F pm_platform_helper >/dev/null 2>&1; then
    pm_platform_helper "$BIN"
fi

# Save files are written relative to original game data, matching old releases.
cd "$DATA"
if [ "${FITD_RELEASE_LOG:-0}" = "1" ]; then
    mkdir -p "$GAMEDIR/logs"
    "$BIN" >> "$GAMEDIR/logs/fitd-release.log" 2>&1
else
    "$BIN" >/dev/null 2>&1
fi

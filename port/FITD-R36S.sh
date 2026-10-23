#!/usr/bin/env bash
# Experimental PortMaster launcher for FITD on R36S ARMHF / dArkOSRE.
# Unzip artifact contents into /roms/ports (or /roms2/ports).
XDG_DATA_HOME="${XDG_DATA_HOME:-$HOME/.local/share}"
if [ -d /opt/system/Tools/PortMaster ]; then
 controlfolder=/opt/system/Tools/PortMaster
elif [ -d /opt/tools/PortMaster ]; then
 controlfolder=/opt/tools/PortMaster
elif [ -d "$XDG_DATA_HOME/PortMaster" ]; then
 controlfolder="$XDG_DATA_HOME/PortMaster"
else
 controlfolder=/roms/ports/PortMaster
fi
export PORT_32BIT=Y
if [ -f "$controlfolder/control.txt" ]; then
 source "$controlfolder/control.txt"
 [ -f "$controlfolder/mod_${CFW_NAME:-unknown}.txt" ] && source "$controlfolder/mod_${CFW_NAME:-unknown}.txt"
 type get_controls >/dev/null 2>&1 && get_controls
else
 echo "PortMaster control.txt not found: $controlfolder"
fi

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
GAMEDIR="$HERE/fitd"
mkdir -p "$GAMEDIR"
exec > >(tee -a "$GAMEDIR/log.txt") 2>&1
BIN="$GAMEDIR/fitd.armhf"
[ -f "$BIN" ] || { echo "Missing $BIN"; exit 1; }
[ -f "$GAMEDIR/gl4es.armhf/libGL.so.1" ] || { echo "Missing GL4ES libGL.so.1"; exit 1; }
[ -d "$GAMEDIR/data" ] || { echo "Missing FITD game data directory"; exit 1; }
if ! find "$GAMEDIR/data" -maxdepth 1 -iname '*.pak' | grep -q .; then
 echo 'Copy the original Alone in the Dark .PAK files to fitd/data/'; exit 1
fi

export LD_LIBRARY_PATH="$GAMEDIR/gl4es.armhf:$GAMEDIR/libs.armhf${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export SDL_GAMECONTROLLERCONFIG="${sdl_controllerconfig:-${SDL_GAMECONTROLLERCONFIG:-}}"
export SDL_VIDEO_GL_DRIVER="$GAMEDIR/gl4es.armhf/libGL.so.1"
export LIBGL_ES="${LIBGL_ES:-2}"
export LIBGL_GL="${LIBGL_GL:-21}"
# Let the firmware select the video backend, rather than hardcoding kmsdrm.
# Avoid bundling/replacing firmware EGL/GLES/Mali and glibc.
echo "FITD on $(uname -m) with $(getconf GNU_LIBC_VERSION 2>&1)"
echo "PortMaster CFW=${CFW_NAME:-unknown} / directory=${directory:-unset}"

mapper_pid=""
cleanup() {
 if type pm_finish >/dev/null 2>&1; then pm_finish
 elif [ -n "$mapper_pid" ]; then kill "$mapper_pid" 2>/dev/null || true; fi
}
trap cleanup EXIT
if [ -n "${GPTOKEYB2:-}" ]; then
 $GPTOKEYB2 "$BIN" -c "$GAMEDIR/fitd.gptk" &
 mapper_pid=$!
elif [ -n "${GPTOKEYB:-}" ]; then
 $GPTOKEYB "$BIN" -c "$GAMEDIR/fitd.gptk" &
 mapper_pid=$!
else
 echo "Warning: PortMaster gptokeyb mapping not available"
fi
if type pm_platform_helper >/dev/null 2>&1; then pm_platform_helper "$BIN"; fi
cd "$GAMEDIR/data" || exit 1
"$BIN"
exit $?

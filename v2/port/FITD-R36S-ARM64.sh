#!/bin/bash
# Experimental native ARM64 port of yaz0r/FITD for R36S/dArkOSRE.
set -u
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
GAME="$ROOT/fitdr36s"
BIN="$GAME/fitd.aarch64"
DATA="$GAME/data"
LOG="$GAME/fitd-r36s.log"
if [ "$(uname -m)" != "aarch64" ]; then
  echo "This FITD build needs an AArch64 Linux userland." | tee "$LOG"
  exit 1
fi
if [ ! -f "$BIN" ]; then
  echo "Missing FITD executable: $BIN" | tee "$LOG"
  exit 1
fi
if [ ! -f "$DATA/LISTBOD2.PAK" ] && [ ! -f "$DATA/PERE.PAK" ] && \
   [ ! -f "$DATA/MER.PAK" ] && [ ! -f "$DATA/AN1.PAK" ] && \
   [ ! -f "$DATA/PURSUIT.PAK" ]; then
  echo "No original game PAK files found in $DATA; AITD1 needs LISTBOD2.PAK." | tee "$LOG"
  exit 1
fi
chmod +x "$BIN"
export SDL_VIDEODRIVER="$(printenv SDL_VIDEODRIVER || echo kmsdrm)"
export SDL_AUDIODRIVER="$(printenv SDL_AUDIODRIVER || echo alsa)"
export SDL_VIDEO_FORCE_EGL=1
export LD_LIBRARY_PATH="$GAME/libs:$(printenv LD_LIBRARY_PATH || true)"
cd "$DATA" || exit 1
echo "FITD AArch64; $(date); SDL driver=$SDL_VIDEODRIVER" > "$LOG"
"$BIN" >> "$LOG" 2>&1
rc=$?
echo "Exit code: $rc" >> "$LOG"
exit "$rc"

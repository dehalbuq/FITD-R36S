#!/bin/sh
# Read-only target check; no firmware modifications.
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
OUT="$ROOT/lakka-rk322x-relatorio.txt"
{
 echo "Date: $(date)"
 echo "Kernel: $(uname -a)"
 echo "glibc:"; getconf GNU_LIBC_VERSION 2>&1 || true
 echo "GPU:"; ls -l /dev/dri /dev/mali* 2>&1 || true
 echo "EGL/GLES/SDL:"; ls -l /usr/lib/libEGL* /usr/lib/libGLES* /usr/lib/libSDL* 2>&1 || true
 echo "ARMHF ELF:"; file "$ROOT/fitd.armhf" 2>&1 || true
 echo "Loader:"; readelf -l "$ROOT/fitd.armhf" 2>&1 | grep -A1 INTERP || true
 echo "Required glibc:"; readelf -W --version-info "$ROOT/fitd.armhf" 2>/dev/null | grep -Eo 'GLIBC_[0-9.]+' | sort -Vu || true
 echo "Runtime libs:"; ldd "$ROOT/fitd.armhf" 2>&1 || true
 echo "Active frontend:"; systemctl is-active retroarch.service emulationstation.service 2>&1 || true
 echo "Last launch:"; tail -120 "$ROOT/fitd-lakka.log" 2>&1 || true
} >"$OUT"
echo "Saved: $OUT"

#!/bin/sh
# Non-destructive Lakka diagnostic: permissions, loader, DRM, audio, game logs.
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
BIN="$ROOT/fitd.armhf"
DATA="$ROOT/data"
OUT="$ROOT/lakka-rk322x-relatorio.txt"
if ! ( : > "$OUT" ) 2>/dev/null; then
    mkdir -p /storage/.cache/fitd-lakka 2>/dev/null || true
    OUT=/storage/.cache/fitd-lakka/lakka-rk322x-relatorio.txt
fi
if ! ( : > "$OUT" ) 2>/dev/null; then
    OUT=/tmp/lakka-rk322x-relatorio.txt
fi
{
 echo "=== FITD Lakka ARMHF Startup Diagnostic ==="
 echo "Date: $(date)"
 echo "User: $(id)"
 echo "Kernel: $(uname -a)"
 echo "Architecture: $(uname -m)"
 echo "glibc:"; getconf GNU_LIBC_VERSION 2>&1 || true
 echo "=== GAME FILES ==="
 ls -ld "$ROOT" "$DATA" "$BIN" 2>&1 || true
 ls -l "$DATA" 2>&1 | head -25 || true
 [ -f "$BIN" ] && echo "binary exists" || echo "binary MISSING"
 [ -x "$BIN" ] && echo "binary executable by current user" || echo "binary not executable (permissions or noexec)"
 [ -r "$BIN" ] && echo "binary readable" || echo "binary not readable"
 [ -w "$ROOT" ] && echo "game directory writable" || echo "game directory not writable"
 [ -x "$DATA" ] && echo "data directory traversable" || echo "data directory not traversable"
 echo "=== MOUNT / NOEXEC ==="
 if command -v findmnt >/dev/null 2>&1; then
  findmnt -T "$BIN" -o TARGET,SOURCE,FSTYPE,OPTIONS 2>&1 || true
  findmnt -T "$DATA" -o TARGET,SOURCE,FSTYPE,OPTIONS 2>&1 || true
 else
  grep -E ' /storage| /media| /roms' /proc/mounts || true
 fi
 echo "=== GPU DEVICES ==="
 ls -ld /dev/dri /dev/dri/* /dev/mali* 2>&1 || true
 echo "=== SOUND DEVICES ==="
 ls -ld /dev/snd /dev/snd/* 2>&1 || true
 echo "=== BINARY/LOADER ==="
 if command -v file >/dev/null 2>&1; then file "$BIN" 2>&1 || true; fi
 if command -v readelf >/dev/null 2>&1; then
  readelf -l "$BIN" 2>&1 | grep -A1 'INTERP' || true
  readelf -W --version-info "$BIN" 2>/dev/null | grep -Eo 'GLIBC_[0-9.]+' | sort -Vu || true
 fi
 ls -l /lib/ld-linux-armhf.so.3 /lib/arm-linux-gnueabihf/ld-linux-armhf.so.3 2>&1 || true
 if command -v ldd >/dev/null 2>&1; then ldd "$BIN" 2>&1 || true; fi
 echo "=== FRONTENDS ==="
 if command -v systemctl >/dev/null 2>&1; then
  systemctl is-active retroarch.service emulationstation.service 2>&1 || true
 fi
 echo "=== LAUNCH LOG ==="
 tail -n 160 "$ROOT/fitd-lakka.log" 2>&1 || true
 tail -n 80 /storage/.cache/fitd-lakka/fitd-lakka.log 2>&1 || true
 echo "=== END ==="
} >"$OUT" 2>&1
echo "FITD diagnostic report: $OUT"
cat "$OUT"

#!/bin/sh
# FITD / PortMaster firmware compatibility report. No proprietary data captured.
set -u
HERE="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
BIN="$HERE/fitd.aarch64"
OUT="$HERE/compatibility-report.txt"
{
    printf 'FITD PortMaster firmware compatibility report\n'
    date -u 2>/dev/null || true
    printf '\nSystem:\n'
    uname -a || true
    cat /etc/os-release 2>/dev/null || true
    printf '\nRuntime:\n'
    getconf GNU_LIBC_VERSION 2>/dev/null || true
    printf 'CFW_NAME=%s\nDEVICE_ARCH=%s\nSDL_VIDEODRIVER=%s\n' \
        "${CFW_NAME:-unknown}" "${DEVICE_ARCH:-unknown}" "${SDL_VIDEODRIVER:-unset}"
    printf '\nFiles:\n'
    ls -l "$BIN" "$HERE/libs.aarch64" 2>&1 || true
    if [ -f "$BIN" ]; then
        printf '\nBinary architecture:\n'
        (file "$BIN" || true) 2>&1
        printf '\nELF library dependencies:\n'
        (readelf -d "$BIN" | grep NEEDED || true) 2>&1
        printf '\nDynamic linker checks:\n'
        (ldd "$BIN" || true) 2>&1
        printf '\nRequired GLIBC symbol versions:\n'
        (readelf -V "$BIN" | grep -o 'GLIBC_[0-9.]*' | sort -Vu || true) 2>&1
    fi
    printf '\nDevice diagnostics:\n'
    (ls /dev/dri 2>/dev/null || true) 2>&1
    printf '\nRenderer selection is not proven by this report.\n'
    printf 'Run the actual game and test controls, audio, saving and exit.\n'
} > "$OUT" 2>&1
printf 'Saved compatibility report: %s\n' "$OUT"

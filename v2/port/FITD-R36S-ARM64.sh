#!/bin/bash
# FITD / R36S / dArkOSRE ARM64 diagnostic launcher.
# FITD_DEBUG=1 (default) collects SDL debug logs and startup stages.
# FITD_GDB=1 optionally uses gdb if installed on the R36S.
set -u
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
GAME="$ROOT/fitdr36s"
BIN="$GAME/fitd.aarch64"
DATA="$GAME/data"
LOGDIR="$GAME/logs"
umask 077
if ! mkdir -p "$LOGDIR" 2>/dev/null; then LOGDIR="$GAME"; fi
LOG="$LOGDIR/fitd-debug.log"
OLD="$LOGDIR/fitd-debug-previous.log"
if [ -f "$LOG" ]; then mv -f "$LOG" "$OLD" 2>/dev/null || true; fi
if ! touch "$LOG" 2>/dev/null; then
    printf 'ERRO: sem permissao para gravar logs em: %s\n' "$LOGDIR" >&2
    exit 20
fi
exec >> "$LOG" 2>&1

export FITD_DEBUG="${FITD_DEBUG:-1}"
export SDL_VIDEODRIVER="${SDL_VIDEODRIVER:-kmsdrm}"
export SDL_AUDIODRIVER="${SDL_AUDIODRIVER:-alsa}"
export SDL_VIDEO_FORCE_EGL=1
export LD_LIBRARY_PATH="$GAME/libs${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

printf '\n=========== FITD-R36S DIAGNOSTICO ===========\n'
date
printf 'Script: %s\nBinario: %s\nDados: %s\n' "$0" "$BIN" "$DATA"
printf 'FITD_DEBUG=%s FITD_GDB=%s SDL_VIDEODRIVER=%s SDL_AUDIODRIVER=%s\n' \
    "$FITD_DEBUG" "${FITD_GDB:-0}" "$SDL_VIDEODRIVER" "$SDL_AUDIODRIVER"
printf '\n== Hardware / ABI ==\n'
uname -a || true
if command -v getconf >/dev/null 2>&1; then getconf GNU_LIBC_VERSION || true; fi
if [ -f /proc/device-tree/model ]; then
    printf 'Device model: '
    tr '\000' ' ' < /proc/device-tree/model
    printf '\n'
fi
if [ -f /proc/meminfo ]; then grep -E '^(MemTotal|MemAvailable):' /proc/meminfo || true; fi
printf '\n== Arquivos e permissoes ==\n'
ls -ld "$ROOT" "$GAME" "$DATA" "$BIN" "$GAME/libs" 2>&1 || true
ls -la /dev/dri /dev/input 2>&1 || true
printf '\n== Biblioteca / loader ==\n'
if command -v file >/dev/null 2>&1; then file "$BIN" || true; fi
if command -v readelf >/dev/null 2>&1; then
    readelf -l "$BIN" 2>&1 | grep -E 'interpreter:|INTERP' || true
fi
if [ -f "$BIN" ] && command -v ldd >/dev/null 2>&1; then
    ldd "$BIN" 2>&1 || true
else
    printf 'ldd indisponivel ou binario ausente\n'
fi
printf '\n== Dados originais encontrados ==\n'
if [ -d "$DATA" ]; then
    find "$DATA" -maxdepth 1 -type f -printf '%f (%s bytes)\n' 2>&1 | head -n 80 || true
fi
printf '\n== Verificacoes ==\n'
if [ "$(uname -m)" != "aarch64" ]; then
    printf 'ERRO: usuario Linux nao e AArch64. Encontrado: %s\n' "$(uname -m)"
    exit 21
fi
if [ ! -f "$BIN" ]; then
    printf 'ERRO: binario inexistente: %s\n' "$BIN"
    exit 22
fi
if [ ! -d "$DATA" ]; then
    printf 'ERRO: diretorio dos dados ausente: %s\n' "$DATA"
    exit 23
fi
found=0
for pak in LISTBOD2.PAK PERE.PAK MER.PAK AN1.PAK PURSUIT.PAK; do
    if [ -f "$DATA/$pak" ]; then found=1; printf 'Detectado: %s\n' "$pak"; fi
done
if [ "$found" -eq 0 ]; then
    printf 'ERRO: nenhum PAK principal encontrado; AITD1 precisa LISTBOD2.PAK\n'
    exit 24
fi
if [ ! -x "$BIN" ]; then chmod +x "$BIN" 2>/dev/null || true; fi
if [ ! -x "$BIN" ]; then
    printf 'ERRO: sem permissao para executar: %s\n' "$BIN"
    exit 25
fi
cd "$DATA" || exit 26
printf '\n== Inicio do executavel ==\n'
date
rc=0
if [ "${FITD_GDB:-0}" = "1" ]; then
    if command -v gdb >/dev/null 2>&1; then
        printf 'Executando com gdb; backtrace automatico na falha\n'
        gdb -q --batch -ex 'set pagination off' -ex run \
            -ex 'thread apply all bt 16' --args "$BIN" || rc=$?
    else
        printf 'AVISO: gdb nao encontrado, executando sem depurador\n'
        "$BIN" || rc=$?
    fi
else
    # Diagnostic build: safety exit to avoid an indefinite black-screen lock.
    # FITD_TEST_TIMEOUT=0 disables the timer for later normal gameplay.
    TEST_TIMEOUT="${FITD_TEST_TIMEOUT:-90}"
    export FITD_VIDEO_TEST="${FITD_VIDEO_TEST:-0}"
    if command -v timeout >/dev/null 2>&1 && [ "$TEST_TIMEOUT" -gt 0 ] 2>/dev/null; then
        printf 'Safety timeout enabled: %s seconds; RGB video test: %s\n' "$TEST_TIMEOUT" "$FITD_VIDEO_TEST"
        timeout --signal=TERM --kill-after=5s "${TEST_TIMEOUT}s" "$BIN" || rc=$?
        if [ "$rc" -eq 124 ]; then
            printf 'FITD-R36S: automatic diagnostic timeout reached; game terminated safely.\n'
        fi
    else
        printf 'Safety timeout disabled or unavailable\n'
        "$BIN" || rc=$?
    fi
fi
printf '\n== Executavel terminou ==\nExit code: %s\n' "$rc"
date
printf 'Log salvo em: %s\n' "$LOG"
exit "$rc"

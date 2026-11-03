#!/usr/bin/env bash
# Regression test: glibc <2.35 uses legacy; glibc >=2.35 preserves dArkOSRE.
# The executable files in this test are harmless shell stubs, not the game.
set -Eeuo pipefail
cd "$(dirname "$0")/.."
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
mkdir -p "$TMP/fitdr36s/data" "$TMP/fitdr36s/libs.aarch64" \
         "$TMP/fitdr36s/libs.glibc230" "$TMP/fakebin"
: > "$TMP/fitdr36s/data/LISTBOD2.PAK"
cp v2/port/FITD-R36S-ARM64-RELEASE.sh "$TMP/FITD-R36S-ARM64.sh"
cat > "$TMP/fakebin/getconf" <<'EOF'
#!/bin/sh
if [ "${1:-}" = GNU_LIBC_VERSION ]; then
  echo "glibc ${FITD_TEST_GLIBC:?}"
else
  /usr/bin/getconf "$@"
fi
EOF
cat > "$TMP/fitdr36s/fitd.aarch64" <<'EOF'
#!/bin/sh
printf "darkosre\n" > "$FITD_TEST_OUTPUT"
EOF
cat > "$TMP/fitdr36s/fitd.glibc230.aarch64" <<'EOF'
#!/bin/sh
printf "arkos4clone\n" > "$FITD_TEST_OUTPUT"
EOF
chmod +x "$TMP/fakebin/getconf" "$TMP/fitdr36s/"*.aarch64
for spec in "2.30:arkos4clone" "2.28:arkos4clone" "2.31:arkos4clone" "2.34:arkos4clone" "2.35:darkosre" "2.39:darkosre"; do
  ver="${spec%%:*}"
  expected="${spec#*:}"
  export FITD_TEST_GLIBC="$ver" FITD_TEST_OUTPUT="$TMP/selected"
  export DEVICE_ARCH=aarch64 FITD_RELEASE_LOG=0
  PATH="$TMP/fakebin:$PATH" bash "$TMP/FITD-R36S-ARM64.sh"
  actual="$(cat "$TMP/selected")"
  [ "$actual" = "$expected" ] || {
    echo "FAIL glibc $ver: expected $expected, got $actual" >&2
    exit 1
  }
  echo "PASS glibc $ver => $actual"
done
rm -f "$TMP/fitdr36s/fitd.glibc230.aarch64"
export FITD_TEST_GLIBC=2.30 FITD_TEST_OUTPUT="$TMP/selected"
if PATH="$TMP/fakebin:$PATH" bash "$TMP/FITD-R36S-ARM64.sh"; then
  echo "FAIL: launcher should refuse incompatible binary without legacy build" >&2
  exit 1
else
  grep -q 'needs ABI-audited' "$TMP/fitdr36s/logs/compatibility-error.txt"
  echo "PASS: old firmware without legacy build exits with clear error"
fi

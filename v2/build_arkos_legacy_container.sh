#!/usr/bin/env bash
# Experimental legacy AArch64 build isolated from the existing dArkOSRE Release.
# Executed INSIDE docker.io/library/debian:buster (glibc 2.28).
# Never run on the handheld, never install/update firmware packages.
set -Eeuo pipefail
cd "$(dirname "$0")/.."
export DEBIAN_FRONTEND=noninteractive

# Debian 10 is end-of-life; these are READ-ONLY archives, not system upgrade repos.
printf '%s\n' \
  'deb [check-valid-until=no] http://archive.debian.org/debian buster main' \
  > /etc/apt/sources.list
rm -f /etc/apt/sources.list.d/*.list
apt-get -o Acquire::Check-Valid-Until=false update
apt-get -y --no-install-recommends install \
  ca-certificates git python3 python3-pil python3-pip fonts-dejavu-core \
  gcc-8 g++-8 make pkg-config ninja-build zip unzip file binutils \
  libasound2-dev libpulse-dev libjack-jackd2-dev \
  libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxfixes-dev \
  libxi-dev libxss-dev libxtst-dev libxkbcommon-dev libdrm-dev \
  libgbm-dev libgl1-mesa-dev libgles2-mesa-dev libegl1-mesa-dev \
  libdbus-1-dev libudev-dev libwayland-dev

# System CMake in Debian 10 is too old for modern SDL3; install a pinned
# CMake wheel WITHOUT upgrading glibc, libstdc++ or Debian runtime packages.
python3 -m pip install --no-cache-dir 'pip==23.3.2'
python3 -m pip install --no-cache-dir 'cmake==3.27.9'
export PATH="/usr/local/bin:$PATH"
export CC=/usr/bin/gcc-8
export CXX=/usr/bin/g++-8

echo "[LEGACY] compiler: $("$CXX" --version | head -n 1)"
echo "[LEGACY] cmake: $(cmake --version | head -n 1)"
echo "[LEGACY] glibc: $(getconf GNU_LIBC_VERSION)"
test "$(uname -m)" = aarch64
test "$(getconf GNU_LIBC_VERSION)" = "glibc 2.28"

FITD_RELEASE=1 FITD_BINARY_ONLY=1 bash v2/build_arm64.sh
test -s work/arkos-legacy/fitd.aarch64
test -s work/arkos-legacy/libs.aarch64/libstdc++.so.6
test -s work/arkos-legacy/libs.aarch64/libgcc_s.so.1

# Ship only additional files; do not overwrite the working dArkOSRE binary.
mv work/arkos-legacy/fitd.aarch64 work/arkos-legacy/fitd.glibc230.aarch64
mv work/arkos-legacy/libs.aarch64 work/arkos-legacy/libs.glibc230
mkdir -p work/arkos-legacy
cat >work/arkos-legacy/README-LEGACY.txt <<'TXT'
FITD-R36S experimental ARM64 legacy ABI build (glibc <=2.30).
Designed for evaluation on ArkOS4Clone (Ubuntu 19.10 / glibc 2.30).
Do not replace the working dArkOSRE variant with these files.
After backing up the port, extract into the existing fitdr36s/ folder.\nIt adds fitd.glibc230.aarch64 and libs.glibc230/; no existing file is replaced.\nRequires the updated launcher that selects glibc230 on older systems.
Compiled from the same source + gameplay patches as dArkOSRE release.
Not a complete PortMaster port or verified multi-firmware build.
TXT
(cd work/arkos-legacy && zip -qyr ../fitd-arkos4clone-arm64-legacy-test.zip .)
test -s work/fitd-arkos4clone-arm64-legacy-test.zip
echo "[LEGACY] Built ABI-audited experimental binary and libraries"

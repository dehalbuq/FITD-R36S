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
apt-cache policy libc6 libc6-dev gcc-8-base gcc-8 g++-8 libgcc-8-dev libstdc++-8-dev libstdc++6 libudev1 || true
# Debian's EOL container has libc6 u3, while the archived libc6-dev is u1.
# Align them ONLY inside this disposable build container; never on handhelds.
apt-get -y --allow-downgrades --no-install-recommends install   libc6=2.28-10+deb10u1
apt-get -y --no-install-recommends -o Debug::pkgProblemResolver=yes install   libc6-dev libstdc++-8-dev gcc-8 g++-8
apt-get -y --no-install-recommends install \
  ca-certificates git python3 python3-pil python3-pip fonts-dejavu-core fonts-dejavu-extra \
  make pkg-config ninja-build zip unzip file binutils \
  libasound2-dev \
  libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxfixes-dev \
  libxi-dev libxss-dev libxtst-dev libxkbcommon-dev libdrm-dev \
  libgbm-dev libgl1-mesa-dev libgles2-mesa-dev libegl1-mesa-dev \
  libdbus-1-dev libwayland-dev

# System CMake in Debian 10 is too old for modern SDL3; install a pinned
# CMake wheel WITHOUT upgrading glibc, libstdc++ or Debian runtime packages.
python3 -m pip install --no-cache-dir 'pip==23.3.2'
python3 -m pip install --no-cache-dir 'cmake==3.27.9'
# Debian Buster Pillow is too old for font.getlength()/baseline anchor.
python3 -m pip install --no-cache-dir 'Pillow==9.5.0'
export PATH="/usr/local/bin:$PATH"
# BGFX/BX explicitly requires GCC >=11 or Clang >=11. GCC 8 from Buster
# cannot compile __builtin_bit_cast and __builtin_is_constant_evaluated.
# Use Clang 11 built for Debian Buster, while retaining Buster glibc 2.28
# and its GCC8/libstdc++ system runtime. These commands affect only Docker.
apt-get -y --no-install-recommends install wget gnupg
wget -qO /tmp/llvm-snapshot.gpg.key https://apt.llvm.org/llvm-snapshot.gpg.key
gpg --batch --yes --dearmor -o /usr/share/keyrings/llvm-snapshot.gpg /tmp/llvm-snapshot.gpg.key
printf '%s\n' 'deb [signed-by=/usr/share/keyrings/llvm-snapshot.gpg] http://apt.llvm.org/buster/ llvm-toolchain-buster-11 main'   > /etc/apt/sources.list.d/llvm-buster.list
apt-get -o Acquire::Check-Valid-Until=false update
apt-cache policy clang-11
apt-get -y --no-install-recommends install clang-11
export CC=/usr/bin/clang-11
export CXX=/usr/bin/clang++-11
# Preflight before cloning/compiling 950+ FITD objects. Report compiler ABI.
cat >/tmp/fitd-clang11-preflight.cpp <<'CPP'
#include <cstdint>
static_assert(__clang_major__ >= 11, "BGFX requires Clang 11+");
constexpr uint32_t number = __builtin_bit_cast(uint32_t, 1.0f);
static_assert(number == 0x3f800000, "Clang bit_cast unavailable");
static_assert(__builtin_is_constant_evaluated(), "constexpr builtin missing");
int main() { return 0; }
CPP
"$CXX" -std=c++20 /tmp/fitd-clang11-preflight.cpp -o /tmp/fitd-clang11-preflight
/tmp/fitd-clang11-preflight
python3 v2/audit_arm64_abi.py --max-glibc 2.30 --max-glibcxx 3.4.28 /tmp/fitd-clang11-preflight
# GCC 8's filesystem ABI is in a separate static library on Buster.
# Check the exact shaderc link requirement before compiling the entire game.
cat >/tmp/fitd-legacy-filesystem.cpp <<'CPP'
#include <filesystem>
int main() {
    auto path = std::filesystem::absolute(std::filesystem::path("."));
    return path.empty();
}
CPP
"$CXX" -std=c++17 /tmp/fitd-legacy-filesystem.cpp -o /tmp/fitd-legacy-filesystem -lstdc++fs
/tmp/fitd-legacy-filesystem
python3 v2/audit_arm64_abi.py --max-glibc 2.30 --max-glibcxx 3.4.28 /tmp/fitd-legacy-filesystem

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
After backing up the port, extract into the existing fitdr36s/ folder.
It adds fitd.glibc230.aarch64 and libs.glibc230/; no existing file is replaced.
Requires the updated launcher that selects glibc230 on older systems.
Compiled from the same source + gameplay patches as dArkOSRE release.
Not a complete PortMaster port or verified multi-firmware build.
TXT
(cd work/arkos-legacy && zip -qyr ../fitd-arkos4clone-arm64-legacy-test.zip .)
test -s work/fitd-arkos4clone-arm64-legacy-test.zip
echo "[LEGACY] Built ABI-audited experimental binary and libraries"

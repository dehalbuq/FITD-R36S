#!/usr/bin/env bash
# Lakka 4.4 RK322x uses glibc 2.32, use Bullseye ARMHF (glibc 2.31).
# Ephemeral build environment only; never modify the target firmware.
set -Eeuo pipefail
cd /workspace
export DEBIAN_FRONTEND=noninteractive
printf '%s\n' \
 'deb [check-valid-until=no] http://archive.debian.org/debian bullseye main' \
 > /etc/apt/sources.list
rm -f /etc/apt/sources.list.d/*.list
apt-get -o Acquire::Check-Valid-Until=false update
# Align Docker's newer security-patched base with the archived development packages.
# These downgrades occur ONLY inside a disposable CI container.
apt-get -y --allow-downgrades --no-install-recommends install \
  libc6=2.31-13+deb11u11 \
  libsepol1=3.1-1 \
  libudev1=247.3-7+deb11u5 \
  perl-base=5.32.1-4+deb11u3
apt-get -y --no-install-recommends install \
  ca-certificates git python3 python3-pil fonts-dejavu-core fonts-dejavu-extra \
  cmake clang-11 ninja-build ccache build-essential pkg-config make \
  file binutils zip unzip \
  libasound2-dev libpulse-dev libudev-dev \
  libx11-dev libxext-dev libxrandr-dev \
  libxcursor-dev libxfixes-dev libxi-dev libxss-dev libxtst-dev libxkbcommon-dev \
  libdrm-dev libgbm-dev libgl1-mesa-dev libgles2-mesa-dev libegl1-mesa-dev \
  libdbus-1-dev libwayland-dev zlib1g-dev \
  libjpeg-dev libfreetype6-dev
cmake --version | head -1
clang++-11 --version | head -1
python3 -c 'from PIL import Image; print("Pillow OK:",Image.__version__)'
# Catch font-resource errors before cloning the large FITD source tree.
mkdir -p /tmp/fitd-font-smoke/FitdLib
python3 lakka/gen_font_buster.py /tmp/fitd-font-smoke
test -s /tmp/fitd-font-smoke/FitdLib/r36sReadableFontData.h
echo 'FITD Lakka font preflight PASS'
test "$(getconf GNU_LIBC_VERSION)" = "glibc 2.31"
if [ "${FITD_DEPS_ONLY:-0}" = "1" ]; then
  echo "ARMHF toolchain image prepared; skipping source compilation."
  exit 0
fi
bash lakka/build_lakka_armhf.sh

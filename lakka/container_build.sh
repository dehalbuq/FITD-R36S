#!/usr/bin/env bash
# Disposable Debian Buster ARMHF/QEMU container. NEVER run on Lakka.
set -Eeuo pipefail
cd /workspace
export DEBIAN_FRONTEND=noninteractive
echo 'deb [check-valid-until=no] http://archive.debian.org/debian buster main' > /etc/apt/sources.list
rm -f /etc/apt/sources.list.d/*.list
apt-get -o Acquire::Check-Valid-Until=false update
apt-get -y --allow-downgrades install libc6=2.28-10+deb10u1
# Archived -dev packages require these EXACT matching runtime versions.
apt-get -y --allow-downgrades install libudev1=241-7~deb10u8 zlib1g=1:1.2.11.dfsg-1+deb10u1
apt-get -y --no-install-recommends install libc6-dev libstdc++-8-dev gcc-8 g++-8 \
  ca-certificates git python3 python3-pip make pkg-config ninja-build zip unzip file binutils \
  wget gnupg libasound2-dev libudev-dev libx11-dev libxext-dev libxrandr-dev \
  libxcursor-dev libxfixes-dev libxi-dev libxss-dev libxtst-dev libxkbcommon-dev \
  libdrm-dev libgbm-dev libgl1-mesa-dev libgles2-mesa-dev libegl1-mesa-dev \
  libdbus-1-dev libwayland-dev libjpeg-dev zlib1g-dev libfreetype6-dev
python3 -m pip install --no-cache-dir 'pip==23.3.2'
python3 -m pip install --no-cache-dir 'cmake==3.27.9' 'Pillow==9.5.0'
export PATH=/usr/local/bin:$PATH
wget -qO /tmp/llvm.gpg https://apt.llvm.org/llvm-snapshot.gpg.key
gpg --batch --yes --dearmor -o /usr/share/keyrings/llvm.gpg /tmp/llvm.gpg
echo 'deb [signed-by=/usr/share/keyrings/llvm.gpg] http://apt.llvm.org/buster/ llvm-toolchain-buster-11 main' > /etc/apt/sources.list.d/llvm.list
apt-get -o Acquire::Check-Valid-Until=false update
apt-get -y --no-install-recommends install clang-11
clang++-11 --version | head -1
bash lakka/build_lakka_armhf.sh

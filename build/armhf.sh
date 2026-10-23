#!/usr/bin/env bash
# Build inside debian:bullseye --platform linux/arm/v7 (ARMHF, glibc 2.31).
set -Eeuo pipefail
export DEBIAN_FRONTEND=noninteractive
ROOT=/workspace
cd "$ROOT"
if [ "$(dpkg --print-architecture)" != armhf ]; then
  echo "FATAL: expected armhf, got $(dpkg --print-architecture)" >&2; exit 2
fi
# Bullseye LTS EOL: official security package indices on the live mirror return 404.
# Use Debian's archived, signed Bullseye and security snapshots instead.
printf '%s\n' \
 'deb [check-valid-until=no] http://archive.debian.org/debian bullseye main' \
 'deb [check-valid-until=no] http://archive.debian.org/debian-archive/debian-security bullseye-security main' \
 > /etc/apt/sources.list
rm -f /etc/apt/sources.list.d/*.list
apt-get -o Acquire::Check-Valid-Until=false update
apt-get install -y --no-install-recommends \
 build-essential qt5-qmake cmake pkg-config python3 git file binutils \
 libsdl1.2-dev libsdl-mixer1.2-dev libsdl2-dev \
 libgl1-mesa-dev libglu1-mesa-dev libegl1-mesa-dev libgles2-mesa-dev \
 libdrm-dev libgbm-dev zlib1g-dev ca-certificates

git clone --depth 1 https://github.com/jmimu/FITD.git FITD
git clone --depth 1 https://github.com/libsdl-org/sdl12-compat.git sdl12-compat
git clone --depth 1 https://github.com/ptitSeb/gl4es.git gl4es

python3 - <<'PY'
from pathlib import Path
p=Path('/workspace/FITD/fitd.pro')
s=p.read_text()
a=[]
for line in s.splitlines():
    if '-m32' in line or '/usr/lib/i386-linux-gnu' in line: continue
    if line.strip().startswith(('LIBS +=','INCLUDEPATH +=')): continue
    if 'INTERNAL_DEBUGGER' in line: continue
    if line.strip() == 'CONFIG += debug': line='CONFIG += release'
    a.append(line)
a += ['CONFIG -= debug','CONFIG -= qt',
      'INCLUDEPATH += /usr/include/SDL',
      'LIBS += -lSDL -lSDL_mixer -lGLU -lGL -lz -lm',
      'QMAKE_CFLAGS += -O2 -fno-strict-aliasing -march=armv7-a -mfpu=neon -mfloat-abi=hard',
      'TARGET = fitd']
p.write_text('\n'.join(a)+'\n')
p=Path('/workspace/FITD/src/osystemSDL_GL.c')
s=p.read_text()
needle='SDL_SetVideoMode(800, 600, 32, SDL_OPENGL/*|SDL_FULLSCREEN*/)'
assert needle in s, 'Upstream video code changed: patch manually'
s=s.replace(needle,'SDL_SetVideoMode(640, 480, 32, SDL_OPENGL | SDL_FULLSCREEN)')
s=s.replace('glViewport (0, 0, 800, 600)','glViewport (0, 0, 640, 480)')
p.write_text(s)
PY

echo '===== FITD BUILD ====='
cd "$ROOT/FITD"
qmake -o Makefile fitd.pro
make -j2
file fitd
readelf -h fitd | grep Machine
mkdir -p "$ROOT/output/fitd/libs.armhf" "$ROOT/output/fitd/gl4es.armhf" "$ROOT/output/fitd/data"
cp fitd "$ROOT/output/fitd/fitd.armhf"

echo '===== SDL12-COMPAT BUILD ====='
cd "$ROOT/sdl12-compat"
cmake -S . -B build-r36s -DCMAKE_BUILD_TYPE=Release -DSDL12TESTS=OFF -DSDL12DEVEL=OFF
cmake --build build-r36s -j2

echo '===== GL4ES BUILD ====='
cd "$ROOT/gl4es"
cmake -S . -B build-r36s -DCMAKE_BUILD_TYPE=Release \
 -DNOX11=ON -DGLX_STUBS=ON -DEGL_WRAPPER=ON -DGBM=OFF
cmake --build build-r36s -j2

echo '===== LIBRARY PACKAGING ====='
python3 - <<'PY'
from pathlib import Path
from collections import deque
import os,shutil,subprocess,re
root=Path('/workspace'); out=root/'output'/'fitd'
libs=out/'libs.armhf'; gl=out/'gl4es.armhf'
candidates=[p for p in (root/'gl4es'/'lib').glob('libGL.so*') if p.is_file() and not p.is_symlink()]
if not candidates: raise SystemExit('No GL4ES libGL.so shared library built')
shutil.copy2(max(candidates,key=lambda p:p.stat().st_size),gl/'libGL.so.1')
sdl=[p for p in (root/'sdl12-compat'/'build-r36s').rglob('libSDL-1.2.so.0*') if p.is_file() and not p.is_symlink()]
if not sdl: raise SystemExit('No SDL12-compat shared library built')
shutil.copy2(max(sdl,key=lambda p:p.stat().st_size),libs/'libSDL-1.2.so.0')
exclude={
 'libc.so.6','libm.so.6','libdl.so.2','libpthread.so.0',
 'librt.so.1','libresolv.so.2','ld-linux-armhf.so.3',
 'libGL.so.1','libEGL.so.1','libGLESv1_CM.so.1','libGLESv2.so.2',
 'libSDL2-2.0.so.0','libdrm.so.2','libgbm.so.1',
}
queue=deque([out/'fitd.armhf',libs/'libSDL-1.2.so.0',gl/'libGL.so.1'])
visited=set(); manifest=[]
pat=re.compile(r'^\s*(\S+)\s+=>\s+(/\S+)')
while queue:
    p=queue.popleft()
    if p.name in visited: continue
    visited.add(p.name)
    env=os.environ.copy()
    env['LD_LIBRARY_PATH']=f'{gl}:{libs}'
    proc=subprocess.run(['ldd',str(p)],env=env,capture_output=True,text=True)
    if proc.returncode!=0 or 'not found' in proc.stdout:
        raise SystemExit(f'Unresolved library for {p}\n{proc.stdout}\n{proc.stderr}')
    for line in proc.stdout.splitlines():
        m=pat.match(line)
        if not m: continue
        soname,src=m.groups()
        if soname in exclude: continue
        dest=libs/soname
        if not dest.exists():
            shutil.copy2(Path(src).resolve(),dest)
            manifest.append(f'{soname} <- {src}')
        queue.append(dest)
(out/'bibliotecas.txt').write_text('\n'.join(manifest)+'\n')
(out/'data'/'INSIRA_JOGO.txt').write_text('Copie aqui seus arquivos originais do Alone in the Dark, incluindo LISTBOD2.PAK. Preserve os nomes originais.\n')
license_dir=out/'licenses';license_dir.mkdir(exist_ok=True)
shutil.copy2(root/'FITD'/'COPYING',license_dir/'FITD-GPL.txt')
for src,name in [(root/'sdl12-compat'/'LICENSE.txt','SDL12-compat-license.txt'),(root/'gl4es'/'LICENSE','GL4ES-license.txt')]:
    if src.is_file():shutil.copy2(src,license_dir/name)
print('Collected',len(manifest),'non-system libraries')
PY

cp "$ROOT/port/FITD-R36S.sh" "$ROOT/output/FITD-R36S.sh"
cp "$ROOT/port/fitd.gptk" "$ROOT/output/fitd/fitd.gptk"
chmod +x "$ROOT/output/FITD-R36S.sh" "$ROOT/output/fitd/fitd.armhf"
echo '===== ELF AND GLIBC AUDIT ====='
file "$ROOT/output/fitd/fitd.armhf" "$ROOT/output/fitd/gl4es.armhf/libGL.so.1"
readelf -l "$ROOT/output/fitd/fitd.armhf" | grep -A1 INTERP || true
for f in "$ROOT/output/fitd/fitd.armhf" "$ROOT"/output/fitd/libs.armhf/*.so* "$ROOT"/output/fitd/gl4es.armhf/*.so*; do
    readelf --version-info "$f" 2>/dev/null | grep -oE 'GLIBC_[0-9.]+' || true
done | sort -Vu | tee "$ROOT/output/fitd/glibc_requerida.txt"
echo 'Done. Runtime still requires dArkOSRE ARMHF glibc/SDL2/EGL/GLES and hardware testing.'

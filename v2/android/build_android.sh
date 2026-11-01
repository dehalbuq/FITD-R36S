#!/usr/bin/env bash
set -Eeuo pipefail
cd "$(dirname "$0")/../.."
ROOT="$PWD"
BASE_SHA="543e22bc78b64405537ae545ecd382dcdf2c12b2"
if [ -z "$ANDROID_HOME" ]; then echo "Missing ANDROID_HOME"; exit 2; fi
export ANDROID_SDK_ROOT="$ANDROID_HOME"
mkdir -p work
rm -rf work/android-source work/android-project work/android-host work/android-out
git clone --recursive https://github.com/yaz0r/FITD.git work/android-source
git -C work/android-source checkout --detach "$BASE_SHA"
git -C work/android-source submodule update --init --recursive
# Copy the SDL3 Gradle front end from the same version as the engine.
cp -a work/android-source/ThirdParty/SDL/android-project work/android-project
mkdir -p work/android-project/app/src/main/java/org/libsdl/app
cp v2/android/FITDImportActivity.java \
  work/android-project/app/src/main/java/org/libsdl/app/FITDImportActivity.java
python3 -m py_compile v2/android/prepare_android.py
python3 v2/android/prepare_android.py work/android-source work/android-project

# shaderc must execute on x86_64 (CI host); it cannot run when built with
# Android NDK for ARMv7. Pre-generate native shader headers first.
echo '== Build shaders and library for host =='
cmake -S work/android-source -B work/android-host -G Ninja \
  -DCMAKE_BUILD_TYPE=Release
cmake --build work/android-host --target FitdLib --parallel 2
find work/android-source/FitdLib/shaders/generated -type f | head -n 15

echo '== Compile Android armeabi-v7a Release APK =='
cd work/android-project
chmod +x ./gradlew
keytool -genkeypair -noprompt -keystore "$PWD/ci-release.keystore" \
  -storepass "$FITD_ANDROID_KEYSTORE_PASSWORD" \
  -keypass "$FITD_ANDROID_KEY_PASSWORD" \
  -alias fitd -keyalg RSA -keysize 2048 -validity 3650 \
  -dname "CN=FITD Community Test,OU=ARMv7,O=Open Source,C=BR" >/dev/null 2>&1
export FITD_ANDROID_KEYSTORE="$PWD/ci-release.keystore"
./gradlew --no-daemon --stacktrace -PBUILD_WITH_CMAKE :app:assembleRelease
cd "$ROOT"
APK="$(find work/android-project/app/build/outputs/apk/release -type f -name '*.apk' | head -n1)"
test -s "$APK"
mkdir -p work/android-out
cp "$APK" work/android-out/FITD-Android-ARMv7-GLES3-Release-TEST.apk
python3 - "$APK" <<'PY'
import sys,zipfile
with zipfile.ZipFile(sys.argv[1]) as f:
    names=f.namelist()
    assert any(s.endswith('/armeabi-v7a/libmain.so') for s in names), 'libmain.so missing'
    assert any(s.endswith('/armeabi-v7a/libSDL3.so') for s in names), 'libSDL3.so missing'
    assert not any(s.upper().endswith(('.PAK','.ITD')) for s in names), 'Game files not allowed'
    print('Android ARMv7 release checked: native libraries present; game data absent')
PY
echo "APK READY: work/android-out/FITD-Android-ARMv7-GLES3-Release-TEST.apk"

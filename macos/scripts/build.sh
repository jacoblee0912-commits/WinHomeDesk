#!/bin/bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"
CMAKE="$ROOT/work/tooling/venv/bin/cmake"
[[ -x "$CMAKE" && -f work/deps/openssl/lib/libcrypto.a && -f work/deps/openh264/lib/libopenh264.a ]] || { echo 'Run macos/scripts/bootstrap.sh first.' >&2; exit 1; }
mkdir -p output/macos
"$CMAKE" -S work/FreeRDP -B work/build-mac -G Ninja \
  -DCMAKE_MAKE_PROGRAM="$ROOT/work/tooling/venv/bin/ninja" \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=27.0 \
  -DCMAKE_EXPORT_NO_PACKAGE_REGISTRY=ON \
  -DOPENSSL_ROOT_DIR="$ROOT/work/deps/openssl" -DOPENSSL_USE_STATIC_LIBS=ON \
  -DWITH_CLIENT=OFF -DWITH_CLIENT_COMMON=ON -DWITH_CLIENT_CHANNELS=ON -DWITH_SERVER=ON -DWITH_SHADOW=ON \
  -DWITH_PROXY=OFF -DWITH_SAMPLE=OFF -DWITH_X11=OFF -DWITH_WAYLAND=OFF \
  -DWITH_FFMPEG=OFF -DWITH_SWSCALE=OFF -DWITH_OPENH264=ON -DWITH_OPENH264_LOADING=OFF -DOPENH264_ROOT="$ROOT/work/deps/openh264" -DWITH_CUPS=OFF \
  -DWITH_PCSC=OFF -DWITH_PKCS11=OFF -DWITH_KRB5=OFF -DWITH_FUSE=OFF \
  -DWITH_MANPAGES=OFF -DWITH_SSE2=OFF -DWITH_WINPR_TOOLS=ON \
  -DWITH_SMARTCARD_EMULATE=OFF -DCHANNEL_URBDRC=OFF -DBUILD_TESTING=OFF
"$CMAKE" --build work/build-mac -j "${BUILD_JOBS:-8}"
xcrun clang -fobjc-arc -O2 -mmacosx-version-min=27.0 \
  -Iwork/FreeRDP/include -Iwork/build-mac/include -Iwork/FreeRDP/winpr/include -Iwork/build-mac/winpr/include \
  macos/App.m macos/Client.m -Lwork/build-mac/libfreerdp -Lwork/build-mac/winpr/libwinpr -Lwork/build-mac/client/common \
  -lfreerdp-client3 -lfreerdp3 -lwinpr3 -framework Cocoa -framework Security -framework ApplicationServices \
  -o output/macos/WinHomeDesk
if [[ "${SKIP_PACKAGE:-0}" != 1 ]]; then python3 macos/scripts/package.py; fi

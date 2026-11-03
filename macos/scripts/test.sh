#!/bin/bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"
APP="${WINHOMEDESK_TEST_APP:-$ROOT/output/macos/WinHomeDesk.app}"
LIB="$APP/Contents/Frameworks"
INC=(-Iwork/FreeRDP/include -Iwork/build-mac/include -Iwork/FreeRDP/winpr/include -Iwork/build-mac/winpr/include)
xcrun clang -fsanitize=address,undefined macos/tests/test_video.c -o output/macos/test_video
output/macos/test_video
xcrun clang -fsanitize=address,undefined macos/tests/test_monitor_layout.c -o output/macos/test_monitor_layout
output/macos/test_monitor_layout
xcrun clang -fsanitize=address,undefined macos/tests/test_audio_pcm.c -framework AudioToolbox -o output/macos/test_audio_pcm
output/macos/test_audio_pcm
xcrun clang -fobjc-arc -O2 -mmacosx-version-min=27.0 "${INC[@]}" macos/tests/TestCursor.m \
  -L"$LIB" -lfreerdp-shadow3.3 -lfreerdp3.3 -lwinpr3.3 -Wl,-rpath,"$LIB" -framework Cocoa -o output/macos/TestCursor
output/macos/TestCursor
xcrun clang macos/tests/test_video_worker.c "${INC[@]}" -L"$LIB" -lwinpr3.3 -Wl,-rpath,"$LIB" -o output/macos/test_video_worker
output/macos/test_video_worker
xcrun clang "${INC[@]}" -Iwork/deps/openh264/include macos/tests/test_encoder_fallback.c \
  work/deps/openh264/lib/libopenh264.a -L"$LIB" -lfreerdp3.3 -lwinpr3.3 -lc++ -Wl,-rpath,"$LIB" \
  -o output/macos/test_encoder_fallback
output/macos/test_encoder_fallback
xcrun clang "${INC[@]}" -Iwork/deps/openh264/include macos/tests/test_vt_interop.c \
  work/FreeRDP/libfreerdp/codec/surface_vt.c work/deps/openh264/lib/libopenh264.a -lc++ \
  -framework VideoToolbox -framework CoreMedia -framework CoreVideo -framework CoreFoundation \
  -o output/macos/test_vt_interop
output/macos/test_vt_interop
xcrun clang "${INC[@]}" macos/tests/test_vt_regions.c -L"$LIB" -lfreerdp3.3 -lwinpr3.3 \
  -Wl,-rpath,"$LIB" -o output/macos/test_vt_regions
output/macos/test_vt_regions
xcrun clang macos/tests/test_helpers.c "${INC[@]}" -L"$LIB" -lwinpr3.3 -Wl,-rpath,"$LIB" -o output/macos/test_helpers
output/macos/test_helpers
xcrun clang -fobjc-arc -O2 -mmacosx-version-min=27.0 macos/tests/TestInputSources.m \
  -framework Foundation -framework Carbon -o output/macos/TestInputSources
output/macos/TestInputSources
xcrun clang -fobjc-arc -O2 -mmacosx-version-min=27.0 "${INC[@]}" \
  macos/tests/TestRemoteKeys.m macos/Client.m -L"$LIB" -lfreerdp-client3.3 -lfreerdp3.3 -lwinpr3.3 -Wl,-rpath,"$LIB" \
  -framework Cocoa -framework ApplicationServices -o output/macos/TestRemoteKeys
output/macos/TestRemoteKeys
xcrun clang -fobjc-arc -O2 -mmacosx-version-min=27.0 "${INC[@]}" \
  macos/tests/TestInputQueue.m macos/Client.m -L"$LIB" -lfreerdp-client3.3 -lfreerdp3.3 -lwinpr3.3 -Wl,-rpath,"$LIB" \
  -framework Cocoa -framework ApplicationServices -o output/macos/TestInputQueue
output/macos/TestInputQueue
xcrun clang -fobjc-arc -O2 -mmacosx-version-min=27.0 "${INC[@]}" \
  macos/tests/TestAppLifecycle.m macos/Client.m -L"$LIB" -lfreerdp-client3.3 -lfreerdp3.3 -lwinpr3.3 -Wl,-rpath,"$LIB" \
  -framework Cocoa -framework Security -framework ApplicationServices -o output/macos/TestAppLifecycle
python3 - <<'PY'
import subprocess
subprocess.run(['output/macos/TestAppLifecycle'], check=True, timeout=8)
PY
python3 macos/tests/test_connection_check.py
if [[ "${1:-}" == --input ]]; then
  xcrun clang -fobjc-arc -O2 -mmacosx-version-min=27.0 "${INC[@]}" \
    macos/tests/InputTest.m macos/Client.m -L"$LIB" -lfreerdp-client3.3 -lfreerdp3.3 -lwinpr3.3 -Wl,-rpath,"$LIB" \
    -framework Cocoa -framework ApplicationServices -o output/macos/InputTest
fi
python3 macos/tests/integration.py "$@"
if [[ "${1:-}" == "" ]]; then python3 macos/tests/video_matrix.py; fi
if [[ "${1:-}" == "" ]]; then
  xcrun clang -fobjc-arc -O2 -mmacosx-version-min=27.0 "${INC[@]}" \
    macos/tests/TestMultiWindow.m macos/Client.m -L"$LIB" -lfreerdp-client3.3 -lfreerdp3.3 -lwinpr3.3 -Wl,-rpath,"$LIB" \
    -framework Cocoa -framework Security -framework ApplicationServices -o output/macos/TestMultiWindow
  python3 macos/tests/media_integration.py
  xcrun clang -fobjc-arc -O2 -mmacosx-version-min=27.0 "${INC[@]}" \
    macos/tests/TestMonitorViews.m macos/Client.m -L"$LIB" -lfreerdp-client3.3 -lfreerdp3.3 -lwinpr3.3 -Wl,-rpath,"$LIB" \
    -framework Cocoa -framework Security -framework ApplicationServices -o output/macos/TestMonitorViews
  python3 macos/tests/monitor_integration.py
fi
codesign --verify --deep --strict "$APP"

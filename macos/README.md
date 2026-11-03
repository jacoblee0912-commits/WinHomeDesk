# WinHomeDesk macOS — 로컬 개발판

대상: Mac mini M4 Pro, macOS 27 / arm64. Windows 0.3.0 GUI·IME 보정 기능을 유지하고, Windows 서버에는 선택적 전체 모니터 공유를 추가했습니다. macOS는 별도 네이티브 AppKit 앱과 FreeRDP shadow backend를 사용합니다. 0.7.0-local 개발 소스 공개용이며 바이너리 릴리스·notarization은 수행하지 않았습니다.

## 빌드와 실행

Xcode의 macOS 27 SDK 및 Python 3가 필요합니다. Homebrew·.NET은 필요하지 않습니다.

```sh
macos/scripts/bootstrap.sh   # 프로젝트 내부에 CMake, Ninja, OpenSSL, OpenH264 준비 (다운로드 필요)
macos/scripts/build.sh
open output/macos/WinHomeDesk.app
```

빌드 산출물은 `output/macos/WinHomeDesk.app`이며 의존 dylib와 라이선스를 포함합니다. `Contents/MacOS` 안의 실행 파일만 분리하지 마세요. 로컬 ad-hoc 서명이며 배포·공증된 앱이 아닙니다. 빌드 경로에 의존하지 않도록 dylib를 번들 상대 경로로 연결합니다.

고정 도구: CMake 3.31.6, Ninja 1.11.1.3, OpenSSL 3.5.4, OpenH264 2.6.0. OpenSSL·OpenH264 공식 소스의 SHA256을 검사합니다. macOS 배포 타깃은 27.0이며 Intel Mac, 이전 macOS는 검증하지 않았습니다.

## Windows에서 Mac에 연결

1. 앱의 **화면·입력 권한 요청**을 사용합니다. 시스템 설정 → 개인정보 보호 및 보안에서 WinHomeDesk의 **화면 및 시스템 오디오 녹음**과 **기기 제어 및 데이터 접근**(Accessibility, 이전 이름 손쉬운 사용)을 허용하고 앱을 재실행합니다. 터미널/Codex에서 실행한 프로세스의 상속 권한과 Finder에서 실행한 앱의 권한은 다를 수 있습니다.
2. **Mac 수신 IP**에 Mac의 LAN IPv4 주소를 입력합니다. `127.0.0.1` 기본값은 로컬 시험 전용입니다.
3. 기본 포트 `3390`, 8자 이상 앱 전용 암호, 공유 동의를 설정하고 **Mac 서버 시작**을 누릅니다.
4. Windows의 RDCMan 서버 주소에 Mac의 LAN IP, 포트 3390, 사용자 `surface`, 빈 Domain을 설정합니다. Mac 로그인 암호 대신 앱 전용 암호를 사용합니다.

5. 서버 인증서 지문을 확인합니다. 생성된 인증서는 `~/Library/Application Support/WinHomeDesk/macOS/shadow/shadow.crt`에 있습니다. 앱의 RDP 파일 저장 기능은 암호를 포함하지 않습니다.
6. 끝나면 **서버 중지**를 누릅니다. 기본 포트를 자동으로 외부에 공개하거나 방화벽/라우터를 수정하지 않습니다.

한/영 전용 키(스캔코드 0x72/0xF2)는 Mac에 이미 활성화된 한국어 입력 소스와 최근 영문 입력 소스를 전환합니다. 클라이언트가 한국어 키보드 레이아웃(0x0412)을 알린 경우 오른쪽 Alt도 같은 동작을 합니다. 다른 레이아웃의 오른쪽 Alt는 Option으로 유지합니다. 키 반복과 누름/뗌은 한 번의 전환으로 처리하며, release-only 한영 키도 지원합니다. 입력 소스를 자동으로 설치하거나 활성화하지는 않습니다.

## 화질·캡처·전송·압축 설정 (0.5.0-local)

오른쪽에서 설정을 고르고 서버를 시작합니다. 서버 실행 중에는 공유 설정을 바꿀 수 없습니다. 설정은 다음 실행에도 유지하며 암호는 저장하지 않습니다.

| 설정 | Mac에서 제공하는 선택지 | Windows와의 대응 |
|---|---|---|
| 전송 화질 | 균형 최대 1920 / 속도 최대 1440 / 원본 픽셀 해상도 | 동일한 긴 변 상한. 코덱 정렬을 위해 각 변을 4의 배수로 맞춤 |
| 목표 FPS | 20 / 30 / 45 | 동일. 실제 전송 FPS를 보장하는 값은 아님 |
| 캡처 방식 | 자동 ScreenCaptureKit 스트림 / 호환 SCScreenshotManager | Windows의 자동 DXGI→GDI / GDI 선택지를 Mac API로 구현 |
| 전송 방식 | AVC420 우선 / 글자 선명 AVC444 우선 / 무손실 색상 GFX Planar / 호환 Bitmap | 같은 FreeRDP 협상·압축·전송 경로. 미지원 수신기는 가능한 하위 모드로 협상 |
| H.264 압축 | GPU 우선·실패 시 CPU / CPU OpenH264 | AVC420은 VideoToolbox 하드웨어 Baseline 인코더 또는 OpenH264. AVC444는 Windows와 동일하게 CPU |

기본값은 **균형 1920 / 20 FPS / 자동 캡처 / AVC420 우선 / GPU 우선**입니다. GPU 표기는 Mac의 VideoToolbox 하드웨어 영상 인코더 사용을 뜻합니다. Planar와 Bitmap에는 H.264 압축 설정이 적용되지 않습니다. 전송은 Windows와 같이 인증·암호화된 RDP/TCP이며 UDP multitransport를 활성화하지 않습니다.

자동 캡처는 스트림 시작·실행 오류가 나면 호환 스크린샷 경로로 전환합니다. 두 경로 모두 같은 화면 기록 권한이 필요하며, 시스템 권한 오류를 우회하지 않습니다. 최신 프레임 한 개만 보관하고 32×32 타일의 RGB 변경을 비교해 중복 화면 전송을 줄입니다. 변경 영역·인코딩·전송 작업은 기존 Windows 공용 파이프라인에 연결했습니다.

GPU AVC420은 비디오 코덱이 복원한 전체 화면을 RDP 갱신 영역으로 보냅니다. 변화가 없는 캡처는 계속 건너뛰며, CPU AVC420의 부분 영역 처리는 유지합니다. H.264 영상 크기는 RDP 규격에 따라 16픽셀 단위로 패딩하고 실제 화면 크기로 잘라 표시합니다.

로그의 `CAPTURE backend`, `ENCODER backend/api`, `CODEC name`은 실제 선택된 방식입니다. `PERF_CAPTURE`는 변경된 화면의 처리율·복사/대기 시간·변경 영역·교체된 프레임 수, `PERF_SEND`는 실제 전송률·인코딩/전송 시간·데이터량을 표시합니다. 정지 화면의 낮은 FPS는 중복 프레임을 보내지 않기 때문일 수 있습니다.

현재 로그인한 주 디스플레이를 공유합니다. 커서는 영상에 합성하지 않고 RDP 포인터 채널로 모양을 전달하며, 수신 PC가 마우스 위치를 즉시 표시합니다. 원본은 논리 포인트가 아닌 디스플레이 모드의 픽셀 크기입니다. 입력 좌표는 Mac의 논리 좌표로 변환합니다. 한 번에 한 명의 제어 클라이언트만 허용하며 잠금·사용자 전환·주 디스플레이 또는 해상도 변경 시 서버를 중단합니다. 잠금 화면, 로그인 화면, 별도 사용자 세션, 무인 서비스는 지원하지 않습니다.

## Mac 시스템 소리와 지연 처리

**Mac 시스템 소리 공유 (마이크 제외)**를 체크하고 서버를 시작하면 Windows RDCMan으로 시스템 재생음을 보냅니다. RDCMan의 원격 오디오는 클라이언트 PC에서 재생하도록 설정해야 합니다. 시스템 소리는 ScreenCaptureKit의 별도 오디오 스트림으로 받으므로 자동/호환 영상 캡처 모두에서 사용할 수 있습니다. 마이크는 캡처하지 않으며 캡처한 소리는 파일에 저장하지 않습니다.

Apple이 지원하는 48 kHz 스테레오를 PCM16으로 바꾸어 **10ms(480프레임, 1920바이트)**씩 전송합니다. Windows 버전처럼 인증·활성화·오디오 협상이 끝난 연결에만 보내고, 메시지 큐 8개 상한과 80ms 초과 패킷 폐기로 밀린 소리가 계속 쌓이지 않도록 합니다. Windows의 기존 44.1 kHz 캡처 경로는 바꾸지 않습니다. `AUDIO_CHANNEL state=ready`, `AUDIO_STATS`, `AUDIO_LATENCY`로 협상과 큐 상태를 확인할 수 있습니다. 이 통계의 서버 처리 시간·클라이언트 보고 대기 시간은 실제 스피커 출력 지연 측정값이 아닙니다.

자동 영상 캡처는 새 프레임 도착 이벤트로 대기를 깨워 처리합니다. 최신 캡처 1개만 유지하며, 별도의 20/30/45 FPS 폴링으로 한 프레임을 더 기다리지 않습니다. 호환 스크린샷 캡처는 선택한 FPS에 맞춰 요청합니다. `PERF_CAPTURE age_ms/age_max_ms`는 캡처 시각부터 처리 시작까지의 시간을 나타냅니다.

Mac 클라이언트는 입력 큐에 이벤트가 들어오면 네트워크 작업자를 바로 깨웁니다. 아직 보내지 않은 연속 마우스 이동은 최신 위치로 합치되, 키·버튼 이벤트를 가로질러 합치지 않습니다. 화면 표시는 60Hz 공통 실행 루프에서 새 이미지가 있을 때만 갱신합니다.

## Mac에서 Windows에 연결

상단 Windows 주소, 포트, 사용자, 선택적 Domain, 암호를 입력하고 **Windows 연결**을 누릅니다. 다른 서버 정보를 입력하고 다시 누르면 기존 창을 유지한 채 별도로 인증하는 새 연결 창을 엽니다. 창 제목에 호스트·포트·연결 상태를 표시합니다.

**같은 주소·포트·사용자·Domain으로 다시 누르면 기존 연결을 공유하는 새 창을 엽니다.** 암호 재입력이 필요 없고, 각 창의 크기와 키 상태는 독립적입니다. 0.7.0에서는 서버가 제공한 모니터 배치를 읽어 아직 열지 않은 모니터를 우선 선택합니다. 원격 창 상단의 **모니터 1 / 모니터 2 / 전체 화면**으로 표시 대상을 바꾸거나 **다른 모니터 새 창**을 누르세요. 메뉴의 **창 → 연결 설정 창**으로 메인 화면을 다시 열 수 있습니다.

Windows WinHomeDesk의 기존 엔진은 주 모니터만 보냅니다. **모니터 2를 보려면 [Windows 엔진 업데이트](windows-monitor-update.md)가 필요합니다.** 확장 모니터 전체를 DXGI 또는 GDI로 캡처하고 표준 RDP 모니터 배치를 전달합니다. Mac은 각 모니터의 화면을 잘라 표시하고 그 위치에 맞는 마우스 좌표를 보냅니다. 왼쪽·위쪽 모니터, 혼합 해상도, 세로 모니터 배치를 처리합니다. 서버가 한 화면만 보내면 모니터 2를 임의로 만들어 표시하지 않습니다.

창의 닫기 버튼과 **선택 창 닫기**는 해당 창만 닫습니다. 같은 연결의 마지막 창을 닫을 때 RDP 연결을 종료하며 다른 PC의 연결은 유지합니다. 앱 종료 시에는 모든 연결과 Mac 공유 서버를 함께 정리합니다. 수신 방식 메뉴는 새로 인증하는 연결에 적용됩니다. 별도 Windows 로그인 세션을 만드는 기능은 아니며, 기존 Windows 서버의 동시 접속 1개 제한도 유지합니다. Mac 서버의 캡처는 현재 주 모니터 1개를 유지합니다. IP와 별칭 호스트명은 서로 다른 주소로 취급하므로 같은 주소 표기를 사용하세요. 새 연결에는 암호가 필요하며 빈 암호로 실패 창을 만들지 않고 입력 안내를 표시합니다.

**접속 진단**은 암호 없이 해당 주소·포트의 TCP 연결과 RDP 보안 협상 응답을 확인합니다. `TCP/RDP 응답 정상 (NLA)`는 서버에 도달했다는 뜻이며 로그인·화면·입력 성공을 의미하지 않습니다. 연결 실패 이유는 창 제목과 메인 창 로그에 표시됩니다.

- Windows 내장 RDP: 서버 기능이 있는 에디션에서 활성화하고 포트 3389 및 해당 Windows 계정을 사용합니다.
- Windows Home의 WinHomeDesk: 기존 Windows 앱에서 서버를 시작하고 포트 3390, 사용자 `surface`, 해당 앱 전용 암호를 사용합니다.
- 인증서 검증 실패 시 지문을 확인하고 해당 연결에만 허용할 수 있습니다. 저장된 인증서 변경은 거부합니다. 일반 연결에 인증서 무시 설정을 쓰지 않습니다.
- 로컬 IME의 확정 문자는 Unicode로 전송하며 단축키·이동키는 스캔코드로 전송합니다. Control → Ctrl, Option → Alt, Command → Windows 키입니다. 원격 창을 벗어나면 누른 키·마우스를 해제합니다.
- Backspace는 삭제 키의 누름·뗌과 반복을 전송합니다. 로컬 한글 IME가 조합 중이면 조합 문자를 먼저 지우고, 원격에 이미 입력된 문자를 동시에 삭제하지 않습니다. Forward Delete는 별도 키로 전송합니다.
- 암호는 파일이나 설정에 저장하지 않습니다. 서버에는 PBKDF2 검증값과 NTLM 호환 해시를 자식 프로세스 환경으로 전달합니다.

`No route to host (errno=65)`가 나타나면 서버 주소·포트·실행 상태와 시스템 설정 → 개인정보 보호 및 보안 → 로컬 네트워크의 WinHomeDesk 허용 상태를 확인합니다. 터미널에서 연결된다는 사실만으로 Finder 앱의 네트워크 권한이 확인되지는 않습니다. Apple은 macOS 로컬 네트워크 권한이 앱 서명을 추적하므로 안정적인 식별을 위해 Apple 발급 코드 서명을 권장합니다. 이 개발판은 ad-hoc 서명이므로 재빌드 후 권한 동작을 별도로 확인해야 합니다. [Apple TN3179](https://developer.apple.com/documentation/technotes/tn3179-understanding-local-network-privacy)

현재 실기에서는 로컬 네트워크의 기존 허용 스위치를 껐다가 다시 켠 뒤 GUI 연결이 정상화됐습니다. 화면·입력 권한도 재빌드 전 항목이 남아 있을 수 있습니다. 이미 허용했는데 계속 권한 안내가 표시되면 해당 설정에서 WinHomeDesk 항목만 제거하고 `output/macos/WinHomeDesk.app`을 다시 추가한 후 앱을 재실행합니다. 다른 앱의 권한은 변경하지 않습니다. 재실행 후 주소·포트·동의는 다시 설정하고 암호는 직접 재입력해야 합니다.

macOS 클라이언트도 오른쪽 **Windows 수신 방식**에서 AVC420·AVC444·Planar·Bitmap을 선택합니다. 다음 연결에 적용되며, Windows 서버의 캡처 화질·FPS·압축 설정은 Windows 앱에서 정합니다. FreeRDP GFX 동적 채널과 VideoToolbox H.264 디코더를 연결했으며, 비트맵 조각마다 전체 화면 이미지를 만들던 처리를 네트워크 이벤트 처리 후 한 번으로 묶었습니다.

키보드/IME 확정 문자열, 마우스, 창 크기에 맞춘 화면 표시를 지원합니다. 실제 Windows WinHomeDesk 서버의 로그인·화면·기본 입력·Backspace 시험은 통과했으며 Windows 내장 RDP 서버는 미검증입니다. Mac 클라이언트의 오디오 재생·클립보드·파일/장치 리다이렉션·동적 원격 해상도는 아직 지원하지 않습니다. Mac 서버의 시스템 소리 공유는 위 방식으로 지원합니다. 기존 Windows 구현의 해당 기능은 제거하지 않습니다.

## 로컬 검증

```sh
macos/scripts/test.sh          # 화질/작업 큐/압축 폴백, 독립 디코더 240프레임, 커서/입력·인증, 영상 8조합, 오디오·다중 창
python3 macos/tests/capture_matrix.py # 실화면: 캡처 2종 × 화질/FPS 3종, GPU 전송
python3 macos/tests/audio_live.py     # 짧은 저음량 시험음: 실제 시스템 캡처→rdpsnd, 녹음 저장 없음
macos/scripts/test.sh --live   # 실제 화면 캡처, localhost 전송 (OS 입력 없음)
macos/scripts/test.sh --input  # 전용 시험 창에서 실제 키·마우스 수신; 실행 중 창 포커스 유지
```

합성 시험은 `127.0.0.1` 바인딩만 허용하며 OS 입력을 주입하지 않습니다. 테스트 암호·인증서는 임시 디렉터리에서 생성 후 제거합니다. 테스트 클라이언트의 인증서 예외는 명시적 테스트 플래그와 주소 `127.0.0.1` 조합에만 적용됩니다. 실제 화면 시험은 스크린샷 파일을 저장하지 않습니다. JSON과 서버 진단 로그는 git에서 제외된 `output/macos`에 남깁니다.

샌드박스가 소켓/TCC 접근을 막는 환경에서는 일반 사용자 터미널에서 실행하세요. `--doctor` 진단은 호출 프로세스의 권한을 보여주며 Finder 앱 권한을 대신 증명하지 않습니다.

## RDCMan 2.93·3.22 검증

두 버전의 실행 파일은 포함하거나 내려받지 않습니다. Windows PC에서 아래 스크립트를 실행합니다.

```powershell
powershell -ExecutionPolicy Bypass -File .\macos\tests\rdcman.ps1 `
  -MacAddress '192.168.0.20' -Port 3390 `
  -RdcMan293 'C:\Tools\RDCMan293\RDCMan.exe' `
  -RdcMan322 'C:\Tools\RDCMan322\RDCMan.exe'
```

스크립트는 실행 파일 버전을 검사하고 버전별 RDG를 만들어 엽니다. 실제 화면 갱신·ASCII/한글·좌우 modifier·더블클릭/드래그/휠·오류 암호·재접속·잠금 중단 결과는 사람이 확인하여 `pass/fail/skip`으로 기록합니다. TCP 접속 성공을 화면·입력 성공으로 간주하지 않습니다. 파일 경로와 바이너리 SHA256을 기록하며 암호는 수집하지 않습니다. 이 스크립트 자체도 Windows 실기에서 아직 실행 검증하지 않았습니다.

같은 Windows 계정에서 3.22와 2.93을 번갈아 실행하면 2.93이 최신 공용 설정·RDG 항목을 읽지 못한다는 경고를 표시할 수 있습니다. 두 앱을 닫고 `%LOCALAPPDATA%\Microsoft\Remote Desktop Connection Manager\RDCMan.settings`와 사용하는 RDG를 먼저 백업하세요. 이전 버전 시험에서 연결 파일 변경을 저장하지 말고, 종료 후 공용 설정을 백업본으로 복원하세요. 검증 스크립트는 공용 설정의 백업·복원을 자동으로 하지 않습니다.

실제 Windows PC에서 RDCMan 2.93·3.22의 Mac 연결·화면 수신·양방향 한영 전환·한글 입력을 확인했습니다. 0.4.0 기본 입력 단계에서 3.22의 Backspace·클릭·드래그·휠도 사용자가 재확인했습니다. 0.5.0 영상 확장의 실기 확인 범위는 검증 문서에 별도로 기록합니다. 기존 Windows용 RDCMan 3.22 IME hook은 수정하지 않았으며 Mac용으로 이식한 것이 아닙니다. 장시간 운영·확장 입력 등 미검증 항목은 검증 문서를 참고하세요.

검증 현황: [VALIDATION.md](VALIDATION.md)

## 구현 구조

- `App.m`: 연결/서버 제어, 권한 안내, 앱 암호 파생, RDP 파일 출력
- `Client.m`: FreeRDP 연결 worker, GDI/GFX 표시, VideoToolbox H.264 수신, AppKit 키/IME/마우스 처리
- `VideoSettings.h`: Windows와 대응하는 영상 옵션 및 서버 인자·환경 설정
- `ConnectionCheck.h`: 인증 정보를 보내지 않는 TCP/X.224 접속 진단
- `../work/FreeRDP/server/shadow/Mac/mac_shadow.m`: ScreenCaptureKit 스트림/스크린샷 최신 버퍼, 해상도·FPS·변경 영역 처리, shadow 전송 worker, Quartz 입력, 인증, 세션 수명 관리
- `scripts/`: 재현 가능한 도구 준비·빌드·번들·테스트

참고: [Apple ScreenCaptureKit](https://developer.apple.com/documentation/screencapturekit/scstream), [Microsoft RDCMan](https://learn.microsoft.com/en-us/sysinternals/downloads/rdcman).

영상 API 참고: [Apple ScreenCaptureKit screenshot capture](https://developer.apple.com/documentation/ScreenCaptureKit/SCScreenshotManager/captureSampleBuffer%28contentFilter%3Aconfiguration%3AcompletionHandler%3A%29), [Apple hardware-required VideoToolbox encoder](https://developer.apple.com/documentation/videotoolbox/kvtvideoencoderspecification_requirehardwareacceleratedvideoencoder), [Cisco OpenH264 2.6.0](https://github.com/cisco/openh264/releases/tag/v2.6.0).

오디오/커서 API 참고: [Apple ScreenCaptureKit 오디오 캡처](https://developer.apple.com/documentation/screencapturekit/scstreamconfiguration/capturesaudio), [지원 오디오 샘플레이트](https://developer.apple.com/documentation/screencapturekit/scstreamconfiguration/samplerate), [시스템 커서 조회](https://developer.apple.com/documentation/appkit/nscursor/currentsystem).

# WinHomeDesk

현재 소스 **0.3.7-local**: 양방향 텍스트·파일 복사/붙여넣기를 추가했습니다. [사용법과 검증 한계](docs/CLIPBOARD.ko.md). 이전 0.3.3 변경: Windows 멀티 모니터 H.264, 모니터별 개별 연결, 환경 설정 오류 및 커서 깜빡임 대응을 포함합니다. [한국어 사용 안내와 검증 범위](docs/WINDOWS_MULTIMONITOR.ko.md) · [English](docs/WINDOWS_MULTIMONITOR.en.md). 실행 파일은 [GitHub Releases](https://github.com/jacoblee0912-commits/WinHomeDesk/releases)에서 버전을 확인해 내려받으세요.

**한국어** | [English](README.en.md)

Windows 11 Home x64에서 현재 로그인한 데스크톱을 공유하는 FreeRDP 기반 독립 RDP 서버입니다. SurfaceDesk라는 이름으로 시작했으며 현재 소스 버전은 **0.3.7-local**입니다.

> 개발 중인 실험 버전입니다. 로그인 전·잠금 화면·UAC 보안 화면·절전 상태 접속은 지원하지 않습니다. Windows 내장 RDP와 별도의 구현이며 독립 Windows 사용자 세션을 생성하지 않습니다.

## 0.3.0: 한국어 / English 선택

메인 화면 오른쪽 위 **Language**에서 표시 언어를 선택할 수 있습니다. 선택은 즉시 적용되고 다음 실행에도 유지됩니다. 서버를 재시작하지 않으며 암호·주소·사용자 그룹 이름은 그대로 보존합니다. 이미 남긴 로그와 엔진 진단 식별자는 다시 번역하지 않습니다.

## 주요 기능

- Unicode 텍스트·파일·폴더 양방향 복사/붙여넣기 (실험 기능, 0.3.7에서 RDCMan 2.93 사용자가 텍스트·파일 전송 성공 확인)

- 주 모니터 / 모든 모니터 통합 / 모니터별 개별 연결 선택
- 모니터별 H.264 전송, 연속 포트 배정, RDCMan 2.93·3.22용 RDG 생성

- DirectX(DXGI) 캡처와 GDI 대체 경로
- H.264 AVC420, AVC444, GFX Planar, 호환 비트맵 전송
- AVC420 GPU 인코딩 우선 및 CPU 대체 경로
- 20 / 30 / 45 FPS, 해상도 축소 전송, 단계별 성능 계측
- 시스템 소리 전송, 키보드·마우스 입력, 오른쪽 Alt 한영 전환
- RDCMan 그룹 파일 생성, RDP 파일 가져오기
- RDCMan 3.22 한글 입력 보정 실행 통합
- 트레이 실행과 로그인 자동 시작, 선택적 관리자 권한 자동 실행

RDPWrap이나 Windows 원격 데스크톱 서비스 패치를 사용하지 않습니다. RDCMan 및 Microsoft SDK 실행 파일은 포함하지 않습니다.

## 설치 및 빠른 시작

배포용 Windows x64 ZIP 전체를 고정된 폴더에 풀고 `WinHomeDesk.exe`를 실행합니다. EXE만 복사하지 마세요.

**접속받는 PC:** 실제 IPv4 주소를 선택하고 앱 전용 암호와 공유 동의를 확인한 뒤 서버를 시작합니다. 사용자 이름은 `surface`, 기본 포트는 TCP `3390`, Domain은 빈칸입니다. Windows PIN이 아닌 앱 전용 암호를 사용합니다. `127.0.0.1`은 같은 PC에서의 시험용 주소입니다.

**접속하는 PC:** 생성한 RDP 파일 또는 RDCMan의 RDG 목록으로 연결합니다. RDCMan 3.22 한글 보정은 관리 PC에서 WinHomeDesk의 **RDCMan 실행 (한글 보정)** 버튼을 사용할 때 적용됩니다. 관리 PC에서는 서버를 시작할 필요가 없습니다.

## 0.2.9 관리자 권한 자동 시작

환경 설정에서 로그인 자동 실행과 관리자 권한 자동 실행을 선택하고 저장합니다. 등록 시 현재 로그인한 관리자 계정의 UAC 승인이 필요합니다. 다음 로그인부터 작업 스케줄러로 실행되며, 제목의 `관리자 권한` 표시로 확인합니다. 서버 자동 시작은 별도 옵션입니다.

**검증 한계:** 설정 전환·오류 복원·UI 시험은 통과했습니다. 제작 환경에서 Task Scheduler COM 접근이 거부되어 실제 예약 작업 등록, UAC 승인 및 재로그인 동작은 검증하지 못했습니다. [검증 결과](docs/검증결과-0.2.9.txt)를 참고하세요.

## 문서

- [0.3.3 멀티 모니터·개별 창 사용 안내](docs/WINDOWS_MULTIMONITOR.ko.md)
- [기본 설치와 사용 안내](docs/GUIDE.ko.md)
- [English installation and usage guide](docs/GUIDE.en.md)
- [설치·사용 설명서 PDF (0.2.8 기준)](docs/WinHomeDesk-0.2.8-설치-사용설명서.pdf) - 관리자 자동 실행은 위의 최신 안내를 우선합니다.
- [빌드 및 테스트](docs/BUILD.md)
- [개발 이력](CHANGELOG.md)
- [설계와 현재 제약](docs/ARCHITECTURE.md)

## 소스 구성

- `work/*.cs`: Windows GUI, 설정, 인증, RDCMan 그룹 관리
- `work/StartupRegistration.cs`: 관리자 로그인 자동 실행과 UAC 등록 도구
- `work/ime-fix`: 사용자에게 동작 확인된 한글 보정 모듈과 소스
- `work/FreeRDP`: 수정된 FreeRDP 소스 전체
- `work/licenses`: 의존성 라이선스와 출처

과거 테스트·패키징 스크립트는 개발 기록으로 보존했습니다. 이전 빌드 폴더나 도구를 전제로 하는 스크립트가 있으므로 현재 빌드는 `work/Build.ps1` 또는 GUI 전용 `work/build_gui030.py`를 사용하세요. 과거 배포물을 재현한 Git 커밋 이력이 아니라 최신 소스 스냅샷과 개발 기록입니다.

## 라이선스

WinHomeDesk 자체 코드와 원본 아이콘은 Apache-2.0입니다. FreeRDP와 각 의존성은 기존 저작권·라이선스를 유지합니다. 자세한 내용은 [LICENSE](LICENSE)와 [NOTICE](work/licenses/NOTICE.txt)를 참고하세요. `ime-fix/WinHomeDesk.ImeHook.dll`은 해당 폴더 소스로 만든 자체 모듈입니다.

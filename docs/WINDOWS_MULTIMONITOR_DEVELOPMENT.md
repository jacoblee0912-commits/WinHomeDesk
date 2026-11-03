# Windows WinHomeDesk 멀티 모니터 개발 이어가기

작성 기준: 2026-11-03, 로컬 개발판 `0.7.0-local`. 이 문서는 다음 개발자가 기존 구현을 이어서 Windows 제품에 통합하기 위한 작업 지침이다. **완료된 엔진 구현과 앞으로 구현할 Windows UI·클라이언트를 구분한다.** 이후 코드와 시험 기록이 바뀌면 이 문서도 갱신한다.

## 1. 목표와 작업 범위

Windows PC의 확장 디스플레이에 있는 **서로 다른 모니터 1·2를 원격에서 각각 보고 조작**할 수 있게 한다. 같은 모니터를 복제한 두 창으로 완료 처리하지 않는다.

1. Windows 서버 GUI에서 `주 모니터만 공유 / 모든 모니터 공유`를 선택하고, 선택한 범위·실제 모니터 수·배치를 확인할 수 있게 한다.
2. Windows 클라이언트에서도 원격 모니터를 선택하고 모니터별 창을 열 수 있게 한다. 같은 서버·계정의 여러 창은 하나의 인증된 연결을 공유하고, 다른 서버는 독립적으로 연결한다.
3. 기존 Windows 기능, Mac 클라이언트의 모니터별 창, RDCMan 2.93·3.22 접속 호환성을 유지한다.
4. 구현·빌드·시험·패키징은 로컬에서 수행한다. GitHub 업로드와 릴리스는 사용자가 명시적으로 요청한 범위에서만 진행한다. **2026-11-03 현재 개발 소스 업로드는 요청됐으며 바이너리 릴리스는 범위에 포함되지 않는다.**

여기서 모니터는 접속 대상 PC의 실제 확장 디스플레이다. 접속하는 PC의 로컬 모니터를 RDP 가상 세션에 배치하는 기능, 여러 Windows 사용자 세션 생성, 여러 제어자의 동시 접속과는 별개다. 이번 단계에서 특정 모니터만 서버가 캡처하는 선택 기능이나 디스플레이 변경의 무중단 적용까지 필수로 확장하지 않는다.

## 2. 시작할 때 확인할 상태

| 항목 | 현재 상태 |
|---|---|
| Windows 서버 엔진 | 전체 확장 디스플레이 캡처·합성·RDP 모니터 배치 전달·입력 좌표 변환 구현 완료 |
| Windows GUI | 기존 `0.3.0` WinForms 앱. 모니터 공유 범위 선택 UI는 **미구현** |
| Windows 클라이언트 | 기존 RDCMan 3.22 실행 및 한글 보정 도우미. 자체 모니터별 원격 창은 **미구현** |
| Mac 클라이언트 | `0.7.0-local`: 모니터 선택, 전체 화면, 다른 모니터 새 창, 연결 공유 구현·실기 확인 |
| Mac 서버 | 화면·입력·시스템 소리 공유 구현. 실제 여러 Mac 디스플레이 캡처는 이번 Windows 구현과 별개이며 현재 주 디스플레이 대상 |
| 실제 Windows 배치 | 모니터 1: 1920×1200, `(0,0)` / 모니터 2: 1920×1200, `(1920,0)` / 합계 3840×1200 |

Windows 엔진은 `SURFACE_DESK_ALL_MONITORS=1`이면 전체, 명시적 `0`이면 주 모니터를 공유한다. 환경 변수가 없으면 **엔진 실행 파일 옆** `WinHomeDesk-all-monitors.flag`의 존재로 전체 공유를 선택한다. 둘 다 없으면 기존처럼 주 모니터만 공유한다. 현재 사용자 PC에는 전체 공유 flag를 포함한 엔진 업데이트가 적용됐다.

Windows의 기존 `/max-connections:1`은 유지돼 있다. 같은 PC의 두 창이 각각 RDP 연결을 만들면 두 번째 연결이 거부될 수 있다. 이를 해결하려고 접속 제한을 풀지 않는다.

### 이미 확인한 결과와 남은 검증

- 실제로 **Mac에서 Windows 서버에 접속**해 모니터 1의 탐색기와 모니터 2의 바탕화면을 서로 다른 창에서 확인했다. 각 창의 우클릭 위치와 Escape 입력도 확인했다.
- 한 창을 닫아도 다른 창이 유지됐고, 같은 연결에서 인증을 반복하지 않고 닫은 모니터 창을 다시 열었다.
- 합성 RDP 시험은 음수 X/음수 Y 배치 × AVC420/Bitmap의 4조합을 통과했다. 두 창의 원본 영역·입력 좌표·마지막 창 종료를 검사한다.
- Windows x64 엔진 교차 빌드, 15개 패키지 파일의 해시·PE 의존성 검사, 실제 Windows `/monitors` 실행을 통과했다.
- **아직 미검증:** Windows의 새 모니터 UI·자체 클라이언트, 실제 Windows 캡처의 DXGI/GDI 강제 모드별 비교, 혼합 DPI·회전·음수 배치 실기, 3대 이상 실기, Windows 내장 RDP 서버에 대한 새 기능.
- RDCMan 2.93·3.22의 Mac 서버 화면·한영 전환·입력·삭제·마우스 검증과 3.22의 소리 검증은 이전 단계에서 통과했다. 이것을 새 Windows 다중 모니터 UI·클라이언트 검증으로 대신하지 않는다.

상세 근거: [기존 검증 기록](../macos/VALIDATION.md). 로컬 결과는 `output/macos/monitor-results.json`, `output/windows-multimonitor/monitor-preflight-actual.txt`, `output/windows-multimonitor/engine-sha256.json`, `output/windows-multimonitor/dependency-audit.json`에 있다. `output`과 도구 폴더는 새 checkout에 없을 수 있다.

## 3. 작업 공간과 소스 인수

Mac과 Windows에서 작업할 소스 checkout 경로를 먼저 확인한다. Windows에서는 **설치된 exe 폴더와 소스 checkout을 분리**하고, 아래 명령은 소스 루트에서 실행한다. 이 공개 문서의 설치 경로는 예시이며 실제 사용자 경로를 포함하지 않는다.

Windows 설치 상위 경로 예시:

```text
C:\Apps\WinHomeDesk
```

실기 검증 당시 실행 파일과 업데이트 폴더:

```text
WinHomeDesk-0.3.0-Windows-x64\WinHomeDesk-0.3.0\WinHomeDesk.exe
WinHomeDesk-MultiMonitor-0.7.0-local\Update-This-PC.cmd
```

검증 대상은 Windows WinHomeDesk 서버, 포트 `3390`, 사용자 `surface`였다. 실제 IP·경로·실행 중인 버전은 새 작업에서 다시 확인한다. 암호와 인증서 개인 키는 이 문서에 포함하지 않는다.

**개발 소스 공개 브랜치는 `codex/macos-multimonitor`다. 새로 clone한 뒤 기본 브랜치만 보지 말고 이 브랜치 또는 그 내용을 포함한 후속 커밋인지 확인한다.** 도구·다운로드·빌드 결과는 Git에서 제외하며, 검증 당시 Windows 의존성 버전·해시는 `macos/dependencies/windows-x64.json`에 보관한다.

1. 적용되는 `AGENTS.md`가 있으면 읽고 `git status --short`, 관련 diff, 아래 파일의 존재를 먼저 확인한다. 이번 작성 시점에는 저장소 내 `AGENTS.md`를 찾지 못했다.
2. Windows로 이어갈 때 수정된 `work/FreeRDP`, `work/*.cs`, `macos` 소스·시험·스크립트·문서를 포함한 브랜치를 checkout한다. 이후 로컬 추가 수정이 있다면 미추적 파일까지 확인한다. 추적 파일의 diff만 전달하면 새 파일이 빠진다.
3. 이전 변경을 reset/clean하거나 수정된 FreeRDP를 upstream 원본으로 덮어쓰지 않는다. 현재 결과·백업을 보존한다.
4. 설치 폴더, 개인 설정, 인증 정보, 인증서 개인 키를 소스 전달물에 넣지 않는다. 필요한 개발 의존성과 자체 IME 모듈은 빌드 문서를 따라 준비한다.

## 4. 먼저 읽을 코드

아래 경로는 모두 저장소 루트 기준이다.

| 영역 | 파일 | 확인할 내용 |
|---|---|---|
| Windows 서버 UI | `work/SurfaceDesk.cs` | `StartServer`, `SetRunning`, `EngineOutput`, 엔진 인자·환경 변수·실행 상태 |
| 설정·트레이·자동 시작 | `work/SurfaceDesk.Lifecycle.cs`, `work/DesktopOptions.cs`, `work/StartupRegistration.cs` | `CurrentOptions`, 로드/저장, DPAPI, 트레이, 일반/관리자 자동 시작 |
| UI 번역 | `work/Localization.cs`, `work/Translations.xml` | 한국어·영어, `L.T`, 전환 시 상태 유지 |
| 기존 Windows 클라이언트 | `work/RdcManClient.cs`, `work/ime-fix/StartupHook.cs` | RDCMan 3.22 전용 보정·실행, 기존 hook 환경 보존 |
| 연결 파일 | `work/RdgManager.cs` | RDG/RDP 입출력·2.93 스키마·자격 증명 제외 |
| Windows 캡처·입력 | `work/FreeRDP/server/shadow/Win/surface_gdi.c`, `surface_dxgi.c`, `surface_dxgi.h` | 장치 열거, 전체 공유 결정, 모니터별 DXGI, GDI 대체, 가상 데스크톱 입력 |
| 공통 좌표 계산 | `work/FreeRDP/server/shadow/surface_monitors.h` | 물리/전송 좌표, 스케일, 음수 원점, 크기 제한 |
| 공통 서버 | `work/FreeRDP/include/freerdp/server/shadow.h`, `work/FreeRDP/server/shadow/shadow_screen.c`, `shadow_client.c` | `surfaceAllMonitors`, 전체 surface, `AdjustMonitorsLayout`, 표준 배치 전달 |
| 이미 동작하는 클라이언트 참고 | `macos/App.m`, `macos/Client.h`, `macos/Client.m` | 연결 공유, monitor popup, 영역 자르기, 입력 좌표·포커스 해제·창 수명 |
| 모니터 시험 | `macos/tests/test_monitor_layout.c`, `TestMonitorViews.m`, `monitor_integration.py` | 실제 함수·뷰·프로토콜을 사용하는 회귀 시험 |
| 빌드·업데이트 | `work/Build.ps1`, `work/build_gui030.py`, `macos/scripts/build-windows-engine.sh`, `package-windows-engine.py`, `Update-WindowsMonitors.ps1` | Windows 네이티브 빌드, Mac 교차 빌드, 엔진 백업·검증·복원 |

### 엔진에서 지켜야 할 좌표 규칙

- `WhdMonitor`의 `right/bottom`은 **exclusive**, RDP `MONITOR_DEF`는 **inclusive**다. 경계 변환을 혼용하지 않는다.
- 실제 캡처 원점은 음수일 수 있다. RDP 배치는 주 모니터를 기준으로 전달하고, 합성 framebuffer는 전체 원점을 뺀 양수 좌표를 사용한다.
- 내부 모니터 배열은 DISPLAY 장치 번호를 기준으로 정렬한다. 배열 인덱스와 Windows 설정 화면의 표시 번호를 무조건 같은 값으로 취급하지 않는다. 진단에는 장치 ID·주 모니터·실제 bounds를 함께 남긴다.
- 현재 제한은 1~16개, 주 모니터 정확히 하나, 전체 전송 화면 각 변 최대 8192다. 화질 상한 1920/1440/원본은 모니터 크기를 기준으로 공통 스케일을 계산하고 배치를 보존한다.
- DXGI는 모니터별 캡처를 합친다. 자동 모드에서 실패하면 전체 가상 데스크톱 GDI로 대체한다. 회전 출력 등 DXGI 미지원 조건의 대체 동작을 유지한다.
- 전체 공유 입력에는 `MOUSEEVENTF_VIRTUALDESK`를 사용한다. 여백과 크기 조절을 거친 창 좌표를 선택 모니터 영역의 원격 좌표로 변환해야 한다.
- 화면 잠금·보안 데스크톱·장치 배치/해상도 변경 시 현재 서버는 중지한다. 오래된 좌표로 입력을 계속 보내지 않는다. 자동 무중단 재배치는 별도 후속 과제다.

## 5. 구현 순서

### 단계 A — Windows 서버 UI와 저장 설정

우선 기존 엔진에 UI를 연결해 사용자에게 flag 파일을 직접 바꾸게 하지 않는다.

1. 모니터 공유 범위 선택과 현재 감지한 장치·해상도·주 모니터 표시를 추가한다. 실행 중에는 범위 변경을 잠그고, 변경 시 중지 후 재시작하도록 안내한다. 화면 공유 동의 문구는 실제 선택 범위를 설명해야 한다.
2. `DesktopOptions`에 범위 설정을 추가하고 유효성 검사·`CurrentOptions`·초기 복원·자동 시작·언어 전환에 모두 연결한다. 기존 `Version=1` 설정과 새 필드가 없는 XML도 읽을 수 있게 한다. 무조건 설정 파일을 폐기하거나 암호를 초기화하지 않는다.
3. **이전 설정의 기본값과 현재 업데이트의 선택을 구분한다.** 예를 들어 `MonitorScope="legacy"`를 신규 필드의 호환 기본값으로 두고, `legacy/primary/all`을 검증한다. 이전 설정은 기존 환경 변수/flag가 결정한 유효 범위를 UI에 정확히 표시한다. 사용자가 명시적으로 선택한 뒤에는 `primary/all`을 저장한다. flag가 없는 기존 설치는 주 모니터 기본값을 유지한다.
4. 명시적 선택은 엔진 자식 프로세스에 `SURFACE_DESK_ALL_MONITORS=0/1`을 전달한다. 시스템·사용자 전역 환경 변수를 수정하지 않는다. `legacy` 상태를 처리할 때도 UI 표시와 실제 엔진 선택이 같아야 한다. 기존 opt-in 설치를 조용히 주 모니터로 되돌리거나, 기존 단일 공유 사용자의 범위를 자동 확대하지 않는다.
5. `/monitors`를 별도 자식 프로세스로 실행해 장치 목록을 얻는다면 실행할 엔진과 동일한 범위·화질 설정을 적용한다. stdout/stderr를 모두 읽고 시간 제한·종료 코드·잘못된 결과를 처리한다. `/monitors`는 목록 확인이며 서버 시작이나 공유 동의를 대체하지 않는다.
6. `SurfaceDesk MONITOR_LIST` 및 `SurfaceDesk MONITOR` 로그를 실제 공유 상태·진단에 반영한다. 감지된 모니터 수와 선택된 공유 모니터 수를 혼동하지 않는다. 이전 엔진이 이 기능을 지원하지 않으면 성공한 것처럼 표시하지 않는다.
7. 모든 새 문구에 한국어/영어 번역을 추가하고 고정 좌표 WinForms UI의 잘림을 점검한다. 모니터 선택이 기존 화질·프레임률·인코더·오디오 선택을 덮어쓰지 않도록 한다.

서버 범위가 실제로 달라지는 환경 변수 전달과 저장/복원까지 시험한 뒤 다음 단계로 진행한다.

### 단계 B — Windows 클라이언트의 모니터별 창

현재 `RdcManClient.cs`는 외부 RDCMan을 실행하는 도우미이며 자체 RDP 화면 뷰가 아니다. 먼저 기존 SDK/공식 API와 로컬 코드를 확인해 실제 지원 범위를 확인한다. RDCMan 전역 설정에 알 수 없는 XML 필드를 추가하거나 `.rdp`의 `use multimon`만으로 원격 물리 모니터별 창이 구현됐다고 판단하지 않는다.

목표 구조는 다음과 같다.

```text
서버·포트·사용자·Domain별 세션 1개
  ├─ 인증 / 인증서 확인 / 디코딩 / 원격 배치 / 입력 전송
  ├─ 원격 창 A: 모니터 1 영역 + 자체 크기·포커스 상태
  └─ 원격 창 B: 모니터 2 영역 + 자체 크기·포커스 상태
```

1. 한 연결의 framebuffer와 모니터 배치를 여러 뷰에서 사용할 수 있는지 작은 실행 가능한 시제품으로 검증한다. 기존 RDCMan API로 불가능하면 현재 FreeRDP를 이용한 Windows 클라이언트 경로를 추가하고, 기존 RDCMan 실행 기능은 유지한다. Windows 내장 RDP 서버나 RDCMan 자체를 패치하지 않는다.
2. 새 FreeRDP 클라이언트가 필요하면 네이티브 엔진과 WinForms 경계를 명확히 한다. UI 스레드에서 네트워크·디코딩을 기다리지 않고, native callback/이미지 버퍼의 소유권·참조 수·종료 순서를 명시한다. 현재 빌드는 `WITH_CLIENT=OFF`이므로 Mac의 client 라이브러리가 Windows 패키지에도 있다고 가정하지 않는다. 새 target과 런타임 의존성을 별도로 빌드·감사한다.
3. Monitor Layout PDU와 GFX ResetGraphics 양쪽에서 원격 배치를 수신하고 유효성을 검사한다. 메타데이터가 없는 구형 단일 모니터 서버는 전체 화면 한 개로 표시한다. 비정상 bounds나 모니터 번호로 버퍼를 읽지 않는다.
4. `전체 화면 / 모니터 1 / 모니터 2 ...`와 `다른 모니터 새 창`을 제공한다. 새 창은 아직 열지 않은 모니터를 우선 선택한다. 제목에는 접속 대상·모니터·연결 상태를 표시한다.
5. 같은 endpoint·계정·Domain의 연결 중/연결 완료 세션을 재사용한다. 다른 계정·Domain에 인증 세션을 공유하지 않는다. 창 닫기는 해당 뷰만 제거하고, 마지막 창 종료 또는 명시적 연결 종료 때 세션을 정리한다. 앱 종료는 모든 작업자의 종료를 기다린다.
6. 모니터 영역을 잘라 그리되 종횡비·여백·DPI를 반영해 입력 좌표를 변환한다. 클릭·드래그·휠·더블클릭·키 입력을 같은 세션으로 보낸다. 포커스를 잃으면 실제로 누른 키/버튼만 마지막 원격 위치에서 해제한다. **무조건 `(0,0)` mouse-up을 보내던 커서 점프를 재도입하지 않는다.**
7. 프레임은 연결 단위로 디코딩하고 각 뷰에 배포한다. 영상 지연을 줄이기 위한 최신 프레임 유지와 이동 이벤트 병합을 보존하고, 클릭·키 이벤트의 순서는 바꾸지 않는다. 같은 연결의 오디오를 창 수만큼 중복 재생하지 않는다.
8. RDCMan 3.22 한글 보정 기능과 2.93 연결 호환성은 각각 유지한다. 3.22 전용 hook을 2.93에 무조건 적용하지 않는다. 자체 클라이언트의 한영 전환·Unicode 조합·Backspace는 별도로 검증한다.

## 6. 기존 기능 보존 조건

- **인증:** NLA/TLS, 인증서 확인, 앱 전용 암호, 접속 제한을 유지한다. 원격 대상에 테스트 인증서 자동 허용 코드를 사용하지 않는다. 암호·NT hash·개인 키를 로그·명령행·문서에 남기지 않는다.
- **암호 저장:** Windows에는 사용자가 서버 자동 시작을 선택한 경우 DPAPI `CurrentUser`로 암호를 보호해 저장하는 기존 기능이 있다. 이를 “암호는 전혀 저장하지 않는다”로 잘못 변경하지 않는다. 평문 저장을 추가하지 않고 기존 보호·복원 동작을 보존한다. Mac 앱의 미저장 방식과 구분한다.
- **Windows 사용성:** 서버 시작/중지·트레이·종료·일반/관리자 자동 시작·언어 전환·진단 기록·RDG/RDP 입출력·한글 보정 실행을 유지한다. 필요 없이 방화벽·작업 스케줄러·RDCMan 공용 설정을 변경하지 않는다.
- **미디어:** 1920/1440/원본, 20/30/45fps, 자동 DXGI/GDI, AVC420/AVC444/GFX Planar/Bitmap, GPU 선호/CPU, Windows WASAPI 소리를 유지한다. 공유 범위가 늘어날 때 실제 캡처/인코딩 시간과 큐 대기를 비교하고 측정 없이 지연 개선을 단정하지 않는다.
- **Mac 회귀:** 공통 FreeRDP를 바꿀 때 Mac 화면·입력·오디오와 두 모니터 창을 회귀 시험한다. VideoToolbox의 Baseline·16픽셀 패딩·GPU 전체 표시 영역 처리와 CPU 부분 갱신을 임의로 제거하지 않는다. 이 부분은 실제 RDCMan 화면 깨짐을 해결한 수정이다.
- **ABI:** `rdpShadowSubsystem`에 `surfaceAllMonitors`가 추가돼 있다. 엔진 exe나 DLL 하나만 교체하지 말고 같은 소스로 빌드한 관련 DLL 전체를 함께 패키징한다.

## 7. 로컬 개발 도구 확인과 빌드

기존 제작 환경: LLVM-MinGW 20261020 UCRT x64, CMake 4.4.4, Ninja 1.13.2, .NET Framework 4.x `csc`; OpenSSL 3.6.5, OpenH264 2.7.0, zlib-ng-compat 2.3.3. **새 환경에서도 이 도구가 설치돼 있다고 가정하지 않는다.** 실제 경로·버전·아키텍처·의존성 해시부터 확인한다. Windows GUI는 .NET Framework 대상이고 IME hook 재빌드의 .NET 10 요구와 다르다.

### Windows 소스 루트

GUI 작업의 기본 명령:

```powershell
python work/build_gui030.py
python work/test029.py
python work/test030.py
```

`build_gui030.py`는 현재 GUI 빌드 진입점이며 이름은 과거 버전명을 유지한다. 새 `.cs` 파일을 추가하면 이 스크립트와 `work/Build.ps1` 양쪽에 포함한다. 출력 `work/WinHomeDesk.exe`만으로는 서버가 동작하지 않는다.

엔진과 GUI 전체 빌드 예시 — 아래 도구 경로는 실제 설치 경로로 바꾼다:

```powershell
& ./work/Build.ps1 -CompilerRoot 'C:/Toolchains/llvm-mingw' -DependencyRoot 'C:/Dependencies/ucrt64' -CMake 'C:/Tools/cmake/bin/cmake.exe' -Ninja 'C:/Tools/ninja.exe'
```

엔진 출력은 `work/build/server/shadow/cli/freerdp-shadow-cli.exe`다. 배포 시 `engine/WinHomeDesk-engine.exe`로 이름을 바꾸고 런타임 DLL·OpenSSL legacy provider·라이선스를 포함한다. `SURFACE_AUDIO_TEST_SOURCE=OFF`를 유지하며 Windows 시스템 DLL과 RDCMan DLL을 재배포하지 않는다.

실제 RDCMan 3.22 도우미 시험은 실행 중인 사용자의 세션을 방해하지 않는 상태에서 별도로 수행한다:

```powershell
$env:RDCMAN_EXE = 'C:/Tools/RDCMan322/RDCMan.exe'
python work/test_client028.py
```

자세한 조건은 [빌드 문서](BUILD.md)와 `work/BUILD-INFO.txt`를 읽는다. 오래된 시험 스크립트가 현재 실행 파일을 과거 소스로 다시 빌드하는지 확인하고 실행한다. 기존 경로·버전을 그대로 가정해 과거 스크립트를 일괄 실행하지 않는다.

### 현재 Mac에서 Windows 엔진 교차 빌드

프로젝트 안의 도구·의존성이 준비된 경우:

```bash
bash macos/scripts/build-windows-engine.sh
work/tooling/venv/bin/python macos/scripts/package-windows-engine.py
```

출력은 `output/windows-multimonitor/`와 `output/WinHomeDesk-Windows-MultiMonitor-0.7.0-local.zip`이다. 현재 패키저는 **엔진 업데이트 전용**이며 새 Windows GUI를 포함하지 않는다. Windows 통합 배포본을 만들 때는 버전·GUI·추가 클라이언트 의존성·업데이트 대상을 명시적으로 확장해야 한다. 같은 출력 폴더에 오래된 파일이 남아 있지 않도록 새 staging 디렉터리에서 패키징한다.

도구가 없으면 `macos/scripts/bootstrap-windows.py`와 공개된 `macos/dependencies/windows-x64.json`을 확인한다. 실행 결과 manifest는 `work/deps-windows/manifest.json`에 생성된다. bootstrap은 MSYS2의 현재 저장소 DB를 조회하므로 재실행 시 의존성 버전이 달라질 수 있다. 기존 검증 버전을 재현할 때는 기록된 파일·SHA256을 기준으로 준비하고, 의존성 업그레이드는 기능 변경과 구분해 검증한다. Mac용 LLVM-MinGW 바이너리를 Windows에서 실행할 수는 없다.

## 8. 시험과 완료 기준

단순 소스 문자열 검사만으로 완료 처리하지 않는다. 설정·좌표·프로세스 실행 조건은 자동 시험으로, 실제 캡처·입력·미디어는 연결 시험으로 확인한다.

| 범위 | 필수 확인 |
|---|---|
| 설정 호환 | 기존 XML 새 필드 없음, 신규 기본값, flag opt-in 유지, 명시적 primary가 flag보다 우선, all 선택, 잘못된 값 거부, 저장/재실행/자동 시작/언어 전환 |
| 서버 UI | 선택과 실제 공유 범위 일치, 실행 중 변경 금지, 서버 중지 후 변경, 구형 엔진·모니터 목록 실패 안내, 한국어/영어·Windows DPI별 잘림 |
| 배치·좌표 | 좌우/상하, 보조가 왼쪽·위쪽인 음수 원점, 주 모니터 순서 변경, 다른 해상도·배율, 세로 화면, 여백·경계 클릭 |
| 두 창 | 서로 다른 화면, 클릭/드래그/휠/더블클릭, 창 크기 변경, 포커스 전환 시 커서 점프·키 눌림 없음, 하나 닫아도 유지, 마지막 닫으면 정리, 재접속 |
| 연결 | 같은 세션 창 생성은 재인증·두 번째 서버 연결 없음, 다른 서버 독립 연결, 잘못된 암호·취소·서버 종료 처리, 인증서 검증 |
| 입력 | `abc 한글 시험 123`, 양방향 한영 전환 반복, Backspace, Ctrl+A, Shift/Ctrl/Alt 해제, 드래그 중 포커스 이탈 |
| 캡처·영상 | 자동 DXGI와 명시적 GDI 각각 실제 두 화면, GPU/CPU AVC420, AVC444 CPU, GFX Planar, Bitmap, 지원하지 않는 경로의 fallback |
| 소리·지연 | 오디오 on/off, 창 수 증가 시 소리 중복 없음, 화면 이동/스크롤 중 끊김·큐 누적, 같은 조건의 단일/전체 모니터 성능 로그 비교 |
| 변경·장애 | 모니터 분리/해상도 변경 시 안전 중지·재시작, 잠금/보안 화면, 네트워크 단절, 앱 종료 시 작업자·캡처·오디오 자원 정리 |
| 기존 호환 | RDCMan 2.93·3.22 연결, Windows 기존 기능, Mac 클라이언트 두 화면·입력, 공통 엔진 수정 시 Mac 서버 미디어 회귀 |
| 패키지 | exe/DLL 버전 일치, 누락 import 없음, 해시·라이선스, 별도 폴더의 설치/업데이트/롤백, 깨끗한 사용자 환경 실행 |

Windows GUI 회귀는 `TestDesktopOptions.cs`, `TestLifecycle017.cs`, `TestStartup029.cs`, `TestLocalization030.cs`를 확장한다. RDG/RDP를 바꾸면 `TestRdg.cs`, `TestRdpImport.cs`, `TestRdcSchema018.cs`도 현재 빌드를 대상으로 실행한다. 모니터 로직은 `surface_monitors.h`의 실제 함수를 호출해 검사하고, 클라이언트는 실제 수신 배치·픽셀·서버에 도달한 입력 좌표로 검증한다.

Mac 공통 경로를 수정했으면 [Mac 빌드 안내](../macos/README.md)에 따라 변경한 소스로 앱과 라이브러리를 먼저 빌드·패키징한 뒤 실행한다:

```bash
bash macos/scripts/test.sh
```

실행 중인 앱을 덮어쓰지 않으려면 `WINHOMEDESK_PACKAGE_PATH`와 `WINHOMEDESK_TEST_APP`을 사용한 별도 `output` staging 경로를 선택한다. 전체 시험은 기존 인증/입력·영상·오디오·여러 창·모니터 4조합을 포함한다. 합성 시험의 통과를 실제 Windows DXGI/GDI·혼합 DPI 실기 통과로 기록하지 않는다.

**완료 조건:** Windows UI에서 전체 공유를 선택해 재실행해도 유지되고, 실제 Windows 서버의 모니터 1·2가 Windows 클라이언트의 서로 다른 창에 표시되며 각 입력이 해당 모니터에 도달한다. 기존 Mac 창과 RDCMan 호환성을 유지하고, 한 창 종료·마지막 창 종료·재연결·업데이트 복원까지 확인한다. 통과/실패/미검증을 나누어 기록하고 실제 실행한 명령·버전·백엔드·배치·로그 위치를 남긴다. 실기 환경이 없는 항목은 미검증으로 남기고 전체 완료로 표현하지 않는다.

## 9. 실제 PC 적용과 복원

현재 검증 PC에는 기존 엔진이 `engine.pre-multimonitor-날짜-시각` 폴더로 백업돼 있다. 정확한 이름은 설치 폴더에서 확인한다. 새 업데이트 전에 다시 백업하고 이전 백업을 덮어쓰지 않는다.

- 실행 중인 엔진을 교체하지 않는다. Windows 현지에서 서버를 중지하면 원격 연결이 끊기는 점을 고려한다. GUI 교체가 포함되면 트레이에 숨긴 상태가 아니라 GUI 프로세스까지 종료해야 한다.
- 적용 전에 staging 산출물·해시·의존성·모니터 열거를 검증한다. 기존 설정과 인증 정보는 유지한다. 현재 `Update-WindowsMonitors.ps1`는 엔진 전용이고 두 모니터 이상을 요구하므로 일반 Windows 설치/GUI 업데이트 도구로 그대로 쓰지 않는다.
- 실패하면 이번에 바꾼 파일만 복원한다. 설정 스키마를 변경했다면 이전 앱으로 되돌릴 때의 호환성과 설정 백업도 확인한다.
- 실제 인증이 필요할 때만 사용자가 앱의 보안 입력란에 암호를 넣도록 한다. 암호를 채팅으로 요청하지 않는다. 주소 입력·창 열기·로그 확인·빌드 등 이미 허용된 작업은 개발자가 이어서 처리한다.
- 완료 시 변경 파일, 로컬 산출물 위치, 시험 결과, 남은 제약, 복원 방법을 보고한다. GitHub 업로드·릴리스는 사용자의 명시적 요청 범위를 따른다.

## 10. 다음 개발 대화에 붙여 넣을 지시문

```text
이 소스 작업 공간의 docs/WINDOWS_MULTIMONITOR_DEVELOPMENT.md를 먼저 읽고 Windows WinHomeDesk 멀티 모니터 개발을 이어서 수행해줘.

이미 구현·검증한 Windows 전체 모니터 캡처 엔진과 Mac 모니터별 창을 기반으로, Windows 서버의 공유 범위 선택 UI·설정 저장을 완성하고 Windows 클라이언트에서도 원격 모니터 1·2를 각각 다른 창으로 보고 조작하게 해줘. 기존 Windows 클라이언트는 RDCMan 3.22 실행 도우미라는 점을 확인하고, 필요한 자체 클라이언트 경로를 검증해 구현해줘. 같은 PC의 여러 창은 한 인증 연결을 공유하고 기존 /max-connections:1을 유지해줘.

먼저 로컬 도구·현재 코드·미커밋 파일·의존성을 확인하고, 기존 Windows 기능과 RDCMan 2.93·3.22·Mac 호환성을 유지하면서 로컬 구현·빌드·테스트까지 진행해줘. 기존 작업을 reset/clean하지 말고, 실행 중인 설치를 덮어쓰지 말아줘. 후속 GitHub 업로드·릴리스는 내가 명시적으로 요청한 범위에서만 진행해줘. 실제 시험과 합성 시험을 구분해 결과·미검증 항목·백업 및 복원 방법을 기록해줘. 암호는 채팅으로 요청하지 말고 필요한 시점에 앱에서 직접 입력하도록 안내해줘.
```

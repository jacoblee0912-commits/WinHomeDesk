# Windows 모니터별 창 업데이트 (0.7.0-local)

Mac 앱만 바꾸면 모니터 2가 나타나지 않습니다. 기존 Windows 서버는 주 모니터만 캡처하므로 이 엔진 업데이트가 필요합니다. Windows 디스플레이 설정은 **확장** 상태여야 합니다.

1. 기존 Windows WinHomeDesk 창에서 **서버 중지**를 누릅니다. 메인 앱은 닫지 않아도 됩니다. 연결 중인 Mac 원격 창은 끊어집니다.
2. 업데이트 ZIP을 완전히 압축 해제합니다. `Update.cmd`를 실행하고 기존 `WinHomeDesk.exe`를 선택합니다. 이 PC 전용 `Update-This-PC.cmd`가 함께 제공됐다면 그것을 실행하면 경로 선택을 생략합니다.
3. `Update complete: 2 monitors detected` 메시지를 확인합니다. 실제 개수가 3개 이상이면 해당 숫자가 표시됩니다.
4. 기존 Windows WinHomeDesk 창에서 **서버 시작**을 누릅니다. 주소·암호·영상·음성 설정은 기존 앱의 설정을 그대로 사용합니다.
5. Mac의 0.7.0 앱으로 연결합니다. 원격 창 상단에서 **모니터 1 / 모니터 2 / 전체 화면**을 선택할 수 있습니다. **다른 모니터 새 창**은 아직 열지 않은 모니터를 우선 엽니다.

업데이트 도구는 먼저 새 엔진의 실행·DLL 로딩·모니터 열거를 확인합니다. 실행 중인 서버가 있거나 확장 디스플레이가 2개 미만이면 기존 파일을 변경하지 않습니다. 파일 SHA256을 검사한 뒤 원래 `engine` 폴더를 `engine.pre-multimonitor-날짜-시각`으로 백업합니다. GUI 실행 파일, 앱 암호, RDCMan 설정, Windows 디스플레이 설정, 방화벽·자동 시작 설정은 수정하지 않습니다.

`engine/WinHomeDesk-all-monitors.flag`가 모든 모니터 공유를 선택합니다. 주 모니터만 공유하려면 서버를 중지한 뒤 이 파일의 이름을 `.disabled`로 바꾸고 재시작하세요. `SURFACE_DESK_ALL_MONITORS=0` 환경 변수가 명시돼 있다면 그 값이 우선합니다. 엔진 자체의 기본값은 기존과 같은 주 모니터 공유입니다.

롤백: 서버를 중지하고 새 `engine` 폴더의 이름을 `engine.multimonitor-disabled`로 바꾼 뒤, 보관된 `engine.pre-multimonitor-…` 폴더를 `engine`으로 이름을 바꾸고 서버를 시작합니다. 백업을 삭제할 필요는 없습니다.

두 창은 하나의 Windows 데스크톱에 속한 서로 다른 모니터를 표시합니다. 별도 사용자 로그인 세션을 만들지 않습니다. 한 창만 닫으면 다른 창은 유지되고, 마지막 창을 닫으면 RDP 연결을 끝냅니다. 장치 연결 해제·배치/해상도 변경·보안 화면 전환 시 서버는 기존과 같이 중지합니다. 디스플레이 설정을 바꿨다면 서버를 다시 시작하세요.

캡처: 모니터별 DXGI Desktop Duplication과 GPU 크기 조절 후 하나의 화면으로 합칩니다. DXGI를 사용할 수 없거나 회전 출력 등 지원하지 않는 조건이면 기존 자동 모드가 가상 데스크톱 GDI 캡처로 전환합니다. 명시적 GDI 모드도 두 모니터를 지원합니다. 영상 코덱과 오디오 처리는 기존 경로를 사용하며, 균형/속도 화질 상한은 개별 모니터 기준으로 적용하고 전체 화면은 각 변 최대 8192픽셀로 제한합니다.

로컬 검증: Windows x64 엔진 교차 빌드·PE 의존성 검사, Mac arm64 앱 빌드·서명 검사, 음수 좌표의 가로/세로 모니터 배치 × AVC420/Bitmap 4조합 인증·화면 분리·서버 입력 좌표 시험 통과. 2026-11-03 업데이트 적용 후 실제 Windows의 1920×1200 모니터 두 개를 Mac의 서로 다른 창에서 확인하고 각 창의 클릭·키 입력·연결 유지도 확인했습니다. DXGI/GDI 강제 모드별 비교, 혼합 DPI·회전 배치, Windows 내장 RDP 서버 실기는 미검증입니다. 상세 결과는 [검증 기록](VALIDATION.md), 후속 Windows UI·클라이언트 작업은 [개발 이어가기 지침](../docs/WINDOWS_MULTIMONITOR_DEVELOPMENT.md)을 참고하세요.

프로토콜은 서버가 원격 모니터 배치를 전달하는 [Microsoft Monitor Layout PDU](https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-rdpbcgr/490b34da-bf5f-4ba2-b080-eefed34706c7)를 사용합니다. 교차 컴파일러는 [LLVM-MinGW 공식 릴리스](https://github.com/mstorsjo/llvm-mingw/releases/tag/20261020), 런타임 라이브러리는 MSYS2 공식 저장소에서 SHA256을 검증해 준비했습니다. GitHub push·릴리스 게시는 수행하지 않습니다.

# 개발 이력

배포 안내와 현재 소스를 기준으로 정리한 이력입니다. 이전 대화 전체나 개인 로그를 공개하지 않으며, 과거 커밋을 인위적으로 재현하지 않습니다.

## 0.3.7-local (2026-11-03)

- Lock incoming file selections when negotiated, tag size/range requests, and unlock after OLE object/stream release; preserve legacy untagged requests.
- Service COM apartment calls while awaiting clipboard responses; retain bounded waits, cancellation and single-request serialization.
- Log unmatched file replies; test real cross-apartment COM waits and DLL lock/tagged-read/unlock packets. User confirmed successful text/file transfer with RDCMan 2.93 after the 0.3.7 update.

## 0.3.6-local (unpublished)

- Receive files with optional descriptor sizes using FILECONTENTS_SIZE; honor FD_ATTRIBUTES and permit empty directory streams.
- Add file request/read failure diagnostics without logging names or contents.
- Add size-query and stream regression tests plus real cliprdr DLL wire checks. Actual client-to-server Explorer retesting remains required.

## 0.3.5-local (Windows, unpublished)
- 수신 텍스트/파일 목록 선검증 및 OLE 콜백의 네트워크 대기 제거
- 최초 클립보드 경합 및 수신 게시 중 재전송 방지, 새 로컬 복사 보존
- 파일 조각 시간 초과가 다른 수신을 영구 차단하지 않도록 수정
- 늦은 형식 응답 분리, Explorer FileContents 형식 조회 호환, 수신 진단 추가
- DLL 실제 패킷 생성/분할 응답 처리와 시간 초과/캐시/경합 회귀 시험 추가

## 0.3.4-local (Windows, unpublished)
- 인증된 RDP 세션의 cliprdr 채널에 Unicode 텍스트 및 파일·폴더 복사 구현
- 전용 STA/OLE 스레드, IDataObject/IStream 부분 읽기, 복사 전용 동작
- 파일 목록 잠금·경로 검증·메모리 한도·시간 초과·진행 중 요청 취소
- 환경 설정 클립보드 사용 옵션, 새 RDP/RDG 클립보드 기본 활성화
- 모니터별 연결에서는 첫 번째 연결이 클립보드 공유 담당
- 프로토콜/COM 및 회귀 시험 통과. 두 PC의 실제 RDCMan 붙여넣기는 미검증
- [한국어 안내](docs/CLIPBOARD.ko.md) / [English](docs/CLIPBOARD.en.md)

## 0.3.3-local (Windows source snapshot, 2026-11-03)
- 환경 설정에서 예약 작업 없음(0x80070002)의 FileNotFoundException 매핑 처리
- GDI 캡처에서 CAPTUREBLT 제거, 프레임/입력마다 디스플레이 모드를 조회하던 경로 제거
- 보이지 않는 WM_DISPLAYCHANGE 감지 창과 USER32 좌표 확인으로 배치 변경 시 중지 유지
- 동일 설정 오류 주입, 이벤트 감지 및 모드 재조회 없는 배치 확인 회귀 시험

## 0.3.2-local (Windows, unpublished)
- 모니터별 개별 연결: 장치별 H.264 캡처·입력 좌표, 연속 포트, RDG 자동 생성
- 여러 엔진의 일괄 시작/중지, 포트 충돌과 예기치 않은 종료 시 정리
- 첫 번째 연결만 소리 전송, 포트별 인증서 지문, 트레이/자동 시작 연동
- 기존/새 Microsoft RDP 엔진으로 가로 2개와 세로 1개 모니터 동시 전송 확인
- [설치 및 개별 창 사용 안내](docs/WINDOWS_MULTIMONITOR.ko.md)

## 0.3.1-local (Windows, unpublished)
- 모니터 범위 선택·저장·동의·진단 UI와 한국어/영어 안내
- Windows 엔진의 다중 DXGI/GDI 캡처, 음수 좌표 입력 변환, 원점 (0,0)의 단일 RDP 캔버스
- RDCMan H.264 전체 화면 전송 및 인코더 패딩 크기 한도 처리
- RDG 화면 맞춤 설정, 기존 설정/암호 보호 유지
- 자세한 시험과 제약: [Windows 안내](docs/WINDOWS_MULTIMONITOR.ko.md)

## 0.3.0
- 한국어/영어 UI 선택, 즉시 적용 및 선택 저장
- 설정·그룹 관리·오류 안내·성능 표시의 280개 문구 번역
- 서버 재시작 없이 언어 변경, 접속 정보와 사용자 입력 보존
- 한국어/영어 README·사용 안내·빌드 설명
- Native RDP engine and Korean input-fix module unchanged from 0.2.9

## 0.2.9
- 작업 스케줄러를 이용한 선택적 관리자 권한 로그인 자동 실행
- 현재 사용자 UAC 승인, 일반 자동 실행과의 전환·해제, 실패 시 복원
- 창 제목에 현재 실행 권한 표시
- 실제 작업 등록·재로그인 시험은 제작 환경 접근 제한으로 미검증

## 0.2.8
- RDCMan 3.22 한글 입력 보정 실행을 WinHomeDesk에 통합
- 내장 모듈 추출, 실행 경로 저장, 보정 로그 확인
- RDCMan 원본 수정 및 구 엔진 강제 전환 없이 새 엔진 유지

## 0.2.7
- 구현되지 않은 UDP multitransport 지원 광고를 제거하여 RDCMan 3.22 초기 연결 대기 수정
- 실제 앱의 Shortpath 초기화 조건을 반영한 연결 시험

## 0.2.6
- 앱 계정의 NLA/CredSSP/NTLMv2 인증과 기존 TLS 경로 호환
- 연결별 인증 상태와 오디오 재접속 처리 개선

## 0.2.4 - 0.2.5
- SurfaceDesk에서 WinHomeDesk로 제품 이름 변경
- 한영키 및 오른쪽 Alt 매핑과 입력 진단 개선

## 0.2.1 - 0.2.3
- GPU AVC420 인코딩과 CPU 대체 경로
- 인코딩 작업과 입력 처리를 분리해 입력 대기 완화
- 시스템 오디오의 샘플링·패킷·타임스탬프 및 재생 확인 처리 개선

## 초기 SurfaceDesk
- Windows Home용 독립 RDP 데스크톱 공유 구현
- DirectX/GDI 캡처, 화면 축소, 전송 방식·FPS 설정과 성능 계측
- 최신 화면 우선 처리, 트레이와 로그인 자동 실행
- RDP 저장, RDCMan RDG 그룹 관리 및 RDP 가져오기

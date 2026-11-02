from pathlib import Path
w=Path('work');s=(w/'사용안내020.txt').read_text(encoding='utf-8-sig')
s=s[s.index('업데이트 후 비교 방법'):]
s=s.replace('3. 자동 DirectX / 자동 AVC420 / 긴 변 1440px / 30FPS','3. 자동 DirectX / 자동 AVC420 / GPU 우선 / 긴 변 1440px / 30FPS')
s=s.replace('특히 PERF_CAPTURE, PERF_PIPELINE, PERF_SEND, H264_STAGE 행','특히 ENCODER, GPU_DEVICE, INPUT, PERF_CAPTURE, PERF_PIPELINE, PERF_SEND, H264_STAGE 행')
s=s.replace("'실제: H.264 AVC420 / CPU' 같은 표시","'실제: H.264 AVC420'과 '사용 중: GPU H.264' 같은 표시")
s=s.replace('- 이번 H.264는 OpenH264 2.7.0 소프트웨어(CPU) 인코더입니다. GPU 하드웨어 H.264 인코딩은 아직 없습니다.','- AVC420는 Windows Media Foundation 하드웨어 인코더를 우선 사용합니다. 실패 시 OpenH264 CPU 인코더로 전환합니다.\n- AVC444/v2는 CPU 인코더를 사용합니다. GPU를 선택해도 클라이언트와 협상한 실제 코덱에 따라 달라집니다.')
s=s.replace('- H.264 목표 전송률은 10 Mbps VBR, 인코더 목표 프레임률은 선택한 20/30/45 FPS에 맞춥니다.','- H.264 목표 전송률은 10 Mbps입니다. GPU는 CBR을 요청하고 CPU 기본값은 VBR입니다. 인코더 목표 프레임률은 선택한 20/30/45 FPS에 맞춥니다.\n  GPU와 CPU는 같은 화질/압축률을 보장하지 않습니다. GPU 경로에서 전송량이 증가할 수 있습니다.')
s=s.replace('권장 비교 시작점: 자동 DirectX + 자동 AVC420 + 1920px + 30 FPS.','비교 시작점: 자동 DirectX + 자동 AVC420 + GPU 우선 + 1440px + 30 FPS.')
s=s.replace('GPU 캡처·축소와 CPU H.264 인코딩은 별개 단계입니다.','GPU 캡처·축소와 GPU/CPU H.264 인코딩은 별개 단계입니다.')
s=s.replace('변경 영역 비교와 CPU 압축·전송 단계가 남아 있습니다.','CPU 메모리 복사·색 변환·변경 영역 비교·전송 단계가 남아 있습니다.')
intro='''Surface Desk 0.2.1 — 입력 처리 분리 / GPU H.264 인코딩

0.2.1 변경 사항
- H.264 압축을 별도 작업 스레드로 옮겼습니다. 연결 스레드는 압축 중에도 마우스·키보드 입력을 처리합니다.
- RDP 통신은 연결 스레드 하나가 담당하여 암호화된 연결에 여러 스레드가 동시에 쓰지 않도록 했습니다.
- 압축 작업은 한 번에 하나만 실행하고 최신 화면 우선 처리와 결합했습니다. 인코딩 대기열을 무제한 늘리지 않습니다.
- GPU 우선(기본): Windows Media Foundation의 하드웨어 H.264 인코더만 선택합니다.
  실제 프레임 인코딩에 성공한 뒤 '사용 중: GPU H.264'를 표시합니다.
- GPU 초기화·인코딩 실패 시 CPU로 자동 전환합니다. 이유와 HRESULT를 로그에 남깁니다.
  GPU 결과 대기는 2초로 제한합니다. 드라이버 자체의 API 호출 정지까지 차단하는 기능은 아닙니다.
- CPU 호환 모드를 직접 선택할 수 있습니다. 변경은 서버 중지 후 적용합니다.
- GPU는 AVC420에 적용합니다. AVC444/v2·Planar·비트맵은 CPU를 사용합니다.
  H.264 이외의 코덱 압축은 이번 작업 스레드 분리 대상에 포함되지 않습니다.
- GPU의 비동기 출력 형식 변경, 첫 프레임 IDR/SPS/PPS, 낮은 지연 설정 및 고해상도 대기를 처리합니다.
- GPU 캡처 결과를 CPU로 읽고 색 변환한 뒤 하드웨어 인코더로 넘기는 구조입니다. 완전한 GPU 무복사 파이프라인은 아닙니다.
- DirectX 실행 중 GDI로 전환할 때 프레임 버퍼와 타이머를 잘못 해제하던 수명 관리도 수정했습니다.
- 기존 0.2.0의 캡처/전달 분리, 대기 화면 최대 1장, 20/30/45FPS, 소리, 트레이, 자동 시작, RDG 기능을 유지합니다.

확인할 표시와 로그
- 영상 압축: GPU 우선 → 접속 후 '사용 중: GPU H.264'인지 확인하세요.
- GPU_DEVICE: 선택한 하드웨어 인코더 이름입니다. 이름만 나온 것은 실제 인코딩 성공을 뜻하지 않습니다.
- ENCODER backend=gpu: GPU 인코딩 성공. backend=cpu reason=...: CPU 전환 이유.
- INPUT_PIPELINE codec_worker=1: 입력 처리와 H.264 압축 분리 활성화.
- INPUT processed=... during_encode=...: 성공적으로 적용한 입력 중 압축과 겹친 입력 횟수.
  입력 좌표, 키 내용, 암호는 기록하지 않습니다. 이 값은 왕복 지연이나 화면 표시 시간 측정값이 아닙니다.
- H264_STAGE codec_ms: GPU/CPU 인코딩 및 해당 결과 대기에 걸린 시간입니다.
- PERF_SEND encode_ms: 준비·인코딩과 그동안 연결 스레드에서 처리한 작업의 대기 시간이 포함됩니다.
- 네트워크 전송이 막히거나 클라이언트 표시가 느리면 입력/화면 지연이 여전히 생길 수 있습니다.
- 실제 Surface의 GPU 드라이버와 LAN 환경별 개선 폭은 아직 측정하지 않았습니다.

'''
(w/'사용안내021.txt').write_text(intro+s,encoding='utf-8')

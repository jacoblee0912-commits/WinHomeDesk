from pathlib import Path
w=Path('work')
intro='''Surface Desk 0.2.2 — 오디오 전송 대기 축소 / 재생 지연 진단

이번 변경
- 오디오 묶음을 20ms(960프레임)에서 10ms(480프레임)로 줄였습니다. 48kHz 스테레오 PCM 형식은 유지합니다.
- 5ms 고해상도 대기로 소리를 확인합니다. Windows 전체 타이머 설정은 변경하지 않습니다.
- WASAPI 버퍼 요청량을 200ms에서 40ms로 줄였습니다. 버퍼 용량은 실제 지연 시간과 같지 않습니다.
- 최신 클라이언트의 Wave2 PCM은 추가 코덱 버퍼를 거치지 않고 보냅니다. 구버전 Wave 전송도 유지합니다.
- Wave2의 오디오 시각을 16비트 값으로 잘라 넘기던 경로를 수정해 32비트 캡처 시각을 전달합니다.
- 캡처 패킷 시각을 이용해 오래된 소리를 판별하고, 전송 전 80ms 이상 지난 패킷은 폐기합니다.
  버퍼를 줄인 만큼 부하가 심한 환경에서는 소리 끊김이 늘 수도 있습니다.
- 서버 구간과 클라이언트 보고 지연을 별도로 기록합니다. 기존 로그로는 고정 지연의 원인을 확정할 수 없습니다.
- 이전 버전의 GPU H.264, 입력 분리, RDG, 트레이, 자동 시작 기능을 유지합니다.

업데이트 및 비교
1. 기존 프로그램은 트레이 메뉴의 '종료 (서버도 종료)'로 완전히 종료합니다.
2. 새 ZIP 전체를 별도 폴더에 압축 해제하고 SurfaceDesk.exe를 실행합니다.
3. 기존과 같은 설정으로 다시 연결하고 말소리/영상이 있는 콘텐츠를 20초 이상 재생하세요.
4. 여전히 늦게 들리면 '로그 저장'으로 새 기록을 남겨주세요. 서버·클라이언트 어느 구간인지 구분할 수 있습니다.
5. 자동 시작을 쓰고 있었다면 새 폴더에서 환경 설정을 저장해 실행 경로를 갱신하세요.

새 계측 해석
- AUDIO_BUFFER actual_ms: 실제 할당된 WASAPI 버퍼 용량. 이 값 자체가 청취 지연은 아닙니다.
- AUDIO_LATENCY server_age_ms: 패킷 캡처 시각부터 서버 전송 호출까지의 대략적인 시간.
  QPC 캡처 시각이 유효하지 않거나 미래를 가리키면 패킷을 읽은 시각으로 대신합니다.
  AUDIO_STATS timestamp_estimates는 해당 대체 횟수이며, 이때 서버 시간에는 읽기 전 대기가 포함되지 않습니다.
  GetTickCount64 기반이므로 짧은 구간은 0ms로 표시될 수 있습니다.
- client_hold_ms: 클라이언트 Wave Confirm 타임스탬프가 보고한 대기/재생 지연의 평균 추정값.
  confirmed=0일 때 0ms는 지연 없음이 아니라 아직 확인되지 않았다는 뜻입니다.
  일부 클라이언트가 같은 패킷을 두 번 확인하므로 패킷별 큰 값을 사용합니다.
- ack_elapsed_ms: 서버 송신부터 확인 수신까지. 클라이언트가 재생 시간을 미리 추정해 회신하면
  client_hold_ms보다 작을 수 있습니다. 이 두 값을 더해 전체 지연으로 해석하지 마세요.
- pending: 최근 약 2초간 전송한 패킷 중 아직 확인 응답을 받지 못한 수. 전체 오디오 대기열 크기는 아닙니다.
- 이 계측은 실제 스피커/블루투스 출력부터 귀까지의 시간을 직접 측정하지 않습니다.
- 화면에는 '소리: 서버 약 ...ms / 클라이언트 보고 ...ms' 또는 확인 대기를 표시합니다.
- 실제 사용 중인 원격 PC에서 고정 지연이 사라졌는지는 아직 검증하지 못했습니다.

이전 버전 기능 안내
'''
old=(w/'사용안내021.txt').read_text(encoding='utf-8')
old=old[old.index('업데이트 후 비교 방법'):]
old=old.replace('rdpsnd 채널로 보냅니다.','rdpsnd 채널로 보냅니다. 오디오 묶음은 10ms입니다.')
(w/'사용안내022.txt').write_text(intro+old,encoding='utf-8')
s=(w/'package021.py').read_text(encoding='utf-8')
s=s.replace('SurfaceDesk-0.2.1','SurfaceDesk-0.2.2').replace('Surface Desk 0.2.1','Surface Desk 0.2.2')
s=s.replace('package021-test','package022-test').replace('사용안내021.txt','사용안내022.txt')
s=s.replace("w/'audio021-results.json'","w/'audio022-results.json'").replace('len(audio)==4','len(audio)==6')
s=s.replace("('audio021-results.json'","('audio022-results.json'")
s=s.replace("assert (w/'validation021-results.json').exists() and (w/'lifecycle021-results.json').exists()","assert (w/'audio022-integration.log').exists() and (w/'audio022-timing.log').exists()")
s=s.replace("assert 'backend=gpu' in (w/'gpu021-server.log').read_text(encoding='utf-8-sig') and 'during_encode=' in (w/'cpu021-server.log').read_text(encoding='utf-8-sig')","assert 'backend=gpu' in (w/'gfx020-avc420.log').read_text(encoding='utf-8-sig',errors='replace')")
start=s.index("validation='''");end=s.index("(pkg/'검증결과.txt')",start)
s=s[:start]+'''validation="""Surface Desk 0.2.2 검증 기록
- WASAPI 실제 캡처 및 48kHz 스테레오 PCM 10ms(1920바이트) 패킷 수신 확인.
- Wave2의 32비트 캡처 시각이 잘리지 않는지 루프백 시각 차이로 확인.
- 구버전 Wave 프로토콜에서도 동일한 10ms PCM 수신 확인.
- 테스트 클라이언트가 350ms 재생 지연을 보고하도록 설정해 서버 계측에서 감지하는지 확인.
  이는 실제 스피커 지연을 350ms로 만든 시험이 아닙니다.
- 두 번 회신하는 ACK, 16비트 타임스탬프 순환, 중복·누락·미일치 회신, 블록 번호 재사용 단위 검사.
- 서버/클라이언트 소리 끄기 및 틀린 암호에서 PCM 미수신 검사.
- GPU AVC420 및 Microsoft RDP ActiveX 영상 연결 회귀 검사.
- 새 오디오 계측 UI의 확인 대기/지연 표시/잘못된 값 처리 검사 및 트레이·설정 회귀 검사.
- 완성 배포 DLL 그대로 제한된 PATH에서 GPU AVC420 및 오디오 채널 연결 확인.
- 개발 PC 테스트는 GDI 캡처 및 재생 없는 테스트 오디오 수신기를 사용합니다.
- 실제 원격 Surface/RDCMan 청취 지연 및 블루투스 장치 지연은 이번 환경에서 측정하지 못했습니다.
- 함께 실린 전체 그래픽·GPU 성능·접속 수명 비교 JSON은 0.2.1에서 수행한 이전 검증 자료입니다.
\\n"""+(w/'audio022-timing.log').read_text(encoding='utf-8')+'\\n'+(w/'audio022-ui.log').read_text(encoding='utf-8')+'\\n'+(w/'audio022-integration.log').read_text(encoding='utf-8')
''' +s[end:]
s=s.replace("names+=['EncoderMode.cs'","names+=['test_audio_timing022.c','test_AudioCaption022.cs','test_audio022.py','validate022.py','build_gui022.py','package022.py','package022_prepare.py','audio022_testgen.py','EncoderMode.cs'")
s=s.replace("0.2.1 additions:","0.2.1 additions:") # retain previous change attribution
s=s.replace("p.write_text(s,encoding='utf-8')","s+='\\n0.2.2 additions: 10ms PCM chunks, WASAPI buffer/poll tuning, full-width Wave2 capture timestamps, duplicate-aware audio confirmation diagnostics.\\n'\np.write_text(s,encoding='utf-8')",1)
s=s.replace('개발 PC 합성 영상 비교 (','0.2.1에서 수행한 개발 PC 합성 영상 비교 (')
s=s.replace('Source package contains all CMake test target source files.','Audio uses 10ms PCM with Wave2 capture timestamps and WaveConfirm diagnostic estimates.\nSource package contains all CMake test target source files.')
(w/'package022.py').write_text(s,encoding='utf-8')

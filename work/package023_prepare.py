from pathlib import Path
w=Path.cwd()/'work'
p=w/'Build.ps1';s=p.read_text(encoding='utf-8-sig');s=s.replace("$disabled=@('WITH_CLIENT'", "$disabled=@('SURFACE_AUDIO_TEST_SOURCE','WITH_CLIENT'");p.write_text(s,encoding='utf-8-sig')
guide='''Surface Desk 0.2.3 — 오디오 재생 속도 / 누적 지연 호환성 수정

이번 변경
- 캡처·패킷·전송 형식을 44.1kHz 스테레오 16비트 PCM으로 통일했습니다.
- 기본 재생 장치의 샘플률이 다르면 Windows 오디오 엔진이 실제 샘플 변환을 수행합니다.
  형식 이름만 바꾸거나 소리를 임의로 빠르게 재생하지 않습니다.
- 10ms마다 441프레임(1764바이트)을 보내며 캡처 시각, 오래된 패킷 폐기, 지연 계측은 유지합니다.
- PCM 협상 시 블록 크기와 초당 바이트 수도 검사합니다.
- 시험음 생성기는 개발용 빌드에서만 사용하며 배포본에서는 비활성화했습니다.

확인한 내용과 한계
- 사용자 로그 22: 서버 구간 약 29ms, 클라이언트 보고 최대 지연이 약 0.3초에서 1.77초까지 증가했습니다.
- 개발 PC의 실제 Microsoft RDP ActiveX에서도 기존 48kHz 경로의 지연 증가를 재현했습니다.
- 44.1kHz 경로의 약 18초 비교 시험에서는 클라이언트 보고 평균 약 125ms, 최대 약 141ms로 안정됐습니다.
- 보낸 440Hz 시험음을 실제 Windows 클라이언트가 재생한 결과를 WASAPI로 측정해 약 440.018Hz를 확인했습니다.
- 이는 해당 클라이언트와 이 프로그램 조합에서의 호환성 개선입니다. Windows의 모든 48kHz RDP가 잘못된다는 뜻은 아닙니다.
- 누적 지연의 재현 조건을 해결했지만 일정한 클라이언트 버퍼 지연은 남습니다.
  실제 Surface의 장시간 재생, 스피커에서의 영상·음성 싱크는 추가 확인이 필요합니다.
- 영상 프레임과 소리의 최종 표시·재생 시점을 강제로 일치시키는 기능은 추가하지 않았습니다.

업데이트 및 확인
1. 기존 프로그램을 트레이 메뉴의 '종료 (서버도 종료)'로 완전히 종료합니다.
2. 새 ZIP 전체를 별도 폴더에 압축 해제하고 SurfaceDesk.exe를 실행합니다. EXE만 바꾸지 마세요.
3. 시스템 소리 전송을 켜고 기존 주소·포트로 다시 연결합니다. RDP/RDG 파일은 다시 만들 필요가 없습니다.
4. 말소리와 영상이 있는 콘텐츠를 1분 이상 재생해 소리 속도와 싱크를 비교하세요.
5. 자동 시작을 사용했다면 새 폴더에서 환경 설정을 다시 저장해 실행 경로를 갱신하세요.
6. 계속 어긋나면 로그를 저장하세요. AUDIO_FORMAT의 rate=44100과 AUDIO_LATENCY 추세가 진단에 사용됩니다.

'''
old=(w/'사용안내022.txt').read_text(encoding='utf-8')
tail=old[old.index('새 계측 해석'):].replace('48kHz / 스테레오','44.1kHz / 스테레오').replace('1.54Mbps','1.41Mbps')
(w/'사용안내023.txt').write_text(guide+tail,encoding='utf-8')
s=(w/'package022.py').read_text(encoding='utf-8')
s=s.replace('0.2.2','0.2.3').replace('package022-test','package023-test').replace('audio022-results','audio023-results').replace('audio022-integration','audio023-integration').replace('사용안내022.txt','사용안내023.txt')
s=s.replace("assert len(gfx)==11", "assert 'SURFACE_AUDIO_TEST_SOURCE:BOOL=OFF' in (w/'build-ready/CMakeCache.txt').read_text()\nassert len(gfx)==11")
s=s.replace("def mean(mode,key):", "assert 'AUDIO_TEST' not in text and 'rate=44100' in text, text[-2000:]\n\ndef mean(mode,key):")
s=s.replace('48kHz 스테레오 PCM 10ms(1920바이트)', '44.1kHz 스테레오 PCM 10ms(1764바이트)')
s=s.replace('- 개발 PC 테스트는 GDI 캡처 및 재생 없는 테스트 오디오 수신기를 사용합니다.', '- 일반 회귀 검사는 GDI 캡처 및 재생 없는 테스트 오디오 수신기를 사용합니다.\n- 별도 시험용 빌드의 440Hz 생성기로 실제 Microsoft RDP ActiveX 재생을 확인했습니다.\n  48kHz 경로는 최대 보고 지연이 약 15초 동안 1.2초 이상으로 증가했고,\n  44.1kHz 경로는 약 18초 시험에서 평균 약 125ms / 최대 약 141ms로 안정됐습니다.\n  실제 재생된 시험음의 WASAPI 측정 결과 440.018Hz를 확인했습니다.\n- 배포본의 시험 생성기 OFF 및 실제 WASAPI 44.1kHz 캡처를 확인했습니다.')
s=s.replace("names+=['test_audio_timing022.c'", "names+=['test_pitch023.c','test_audio023.py','validate023.py','build_gui023.py','package023.py','package023_prepare.py','사용안내023.txt','test_audio_timing022.c'")
s=s.replace("s+='\\n0.2.3 additions: 10ms", "s+='\\n0.2.3 additions: 44.1kHz PCM capture/packet/negotiation compatibility, actual Microsoft playback regression; 10ms")
s=s.replace('Audio uses 10ms PCM', 'Production builds force SURFACE_AUDIO_TEST_SOURCE=OFF. Audio uses 44.1kHz stereo 16-bit, 10ms PCM')
(w/'package023.py').write_text(s,encoding='utf-8')

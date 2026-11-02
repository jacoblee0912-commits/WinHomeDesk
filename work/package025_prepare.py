from pathlib import Path
w=Path.cwd()/'work'
guide='''WinHomeDesk 0.2.5 — 오른쪽 Alt로 전달되는 한영키 수정

이번 변경
- 환경 설정에 '오른쪽 Alt를 한영키로 사용'을 추가했습니다. 기본값은 켜짐입니다.
- 이전 버전은 원격 전경 창의 키보드 배치와 MapVirtualKey 결과가 모두 한영키를 가리킬 때만 변환했습니다.
  이번 옵션을 켜면 해당 판별에 관계없이 RDP의 오른쪽 Alt(E0 38)를 한영 전환 키로 전달합니다.
- 누름/뗌을 한 쌍으로 보내고 반복 입력과 짝이 되는 뗌은 무시해 두 번 전환되는 현상을 방지합니다.
- 왼쪽 Alt와 일반 키는 바꾸지 않습니다. 일반 오른쪽 Alt/AltGr가 필요하면 옵션을 해제하세요.
- INPUT_ALT에 오른쪽 Alt 수신, 설정, 원격/클라이언트 배치와 처리 결과를 기록합니다.
  INPUT_IME에는 한영키 변환 후 Windows 입력 API에 전달한 결과를 기록합니다. 일반 문자/암호는 기록하지 않습니다.
- 기존 설정 파일에 새 항목이 없으면 기본 켜짐으로 읽습니다. 계정·암호·인증서·RDP/RDG 파일은 유지됩니다.

업데이트
1. 기존 WinHomeDesk를 트레이 메뉴의 '종료 (서버도 종료)'로 완전히 종료합니다.
2. 새 ZIP 전체를 새 폴더에 풀고 WinHomeDesk.exe를 실행합니다. EXE만 교체하지 마세요.
3. 환경 설정의 '오른쪽 Alt를 한영키로 사용'이 켜져 있는지 확인합니다.
4. 서버를 시작하고 기존 RDP 또는 RDCMan으로 다시 접속합니다.
5. 원격 서피스에서 한국어 Microsoft IME를 선택하고 입력란을 클릭한 뒤 한영키를 누릅니다.
6. 옵션을 바꾼 경우 설정 저장 후 서버를 중지하고 다시 시작해야 적용됩니다.
7. 자동 시작을 사용했다면 새 프로그램의 환경 설정을 저장해 실행 경로를 갱신하세요.

검증과 남은 확인
- 옵션 ON: 오른쪽 Alt RDP 입력을 VK_HANGUL 누름/뗌으로 변환하는 경로를 검사했습니다.
- 옵션 OFF: 일반 오른쪽 Alt/AltGr 스캔 코드를 유지하는 경로를 검사했습니다.
- 반복 입력, 전경 키보드 배치가 바뀐 뒤의 키 뗌, 왼쪽 Alt/오른쪽 Ctrl/일반 문자, 기존 한영·한자키를 검사했습니다.
- 기존 설정 파일 로드, 새 옵션 저장, 암호화, 트레이 기능 및 GPU AVC420 연결 회귀 검사를 수행했습니다.
- 실제 사용자 Surface의 한국어 IME 전환·조합은 아직 확인하지 못했습니다.
- 계속 작동하지 않으면 한영키를 두세 번 누른 직후 로그를 저장하세요.
  INPUT_ALT가 없으면 해당 키가 오른쪽 Alt 형태로 서버에 도착했는지부터 확인해야 합니다.
  INPUT_IME의 injected=2는 Windows API가 입력을 받았다는 뜻이며 최종 IME 전환 성공을 보장하지는 않습니다.

기존 기능 안내
'''
old=(w/'사용안내023.txt').read_text(encoding='utf-8');tail=old[old.index('새 계측 해석'):].replace('Surface Desk','WinHomeDesk').replace('SurfaceDesk.exe','WinHomeDesk.exe')
(w/'사용안내025.txt').write_text(guide+tail,encoding='utf-8')
s=(w/'package024.py').read_text(encoding='utf-8').replace('0.2.4','0.2.5').replace('package024-test','package025-test')
s=s.replace('사용안내024.txt','사용안내025.txt').replace('keyboard024-test.log','keyboard025-test.log').replace('graphics024-input.log','graphics025-ralt-on.log').replace('gui024-tests.txt','gui025-tests.txt')
s=s.replace("assert 'INPUT_IME key=hangul' in (w/'gfx020-avc420.log').read_text(errors='replace')", "on=(w/'ralt025-on-server.log').read_text(errors='replace');off=(w/'ralt025-off-server.log').read_text(errors='replace')\nassert on.count('INPUT_IME key=hangul scan=0x38 release=0 injected=2 expected=2')==2\nassert 'mode=hangul' in on and 'mode=layout' in off and 'action=scancode' in off and 'INPUT_IME' not in off")
s=s.replace("names+=['test_keyboard024.c'", "names+=['test_keyboard025.c','build_gui025.py','test_gui025.py','package025.py','package025_prepare.py','사용안내025.txt','test_keyboard024.c'")
s=s.replace(".replace('Surface Desk 0.2.0','Surface Desk 0.2.3')", ".replace('Surface Desk 0.2.0','WinHomeDesk 0.2.5')")
s=s.replace('0.2.5: WinHomeDesk branding, Korean IME keyboard translation;', '0.2.5: configurable right Alt to Hangul mapping independent of foreground layout, diagnostic traces;')
s=s.replace('한영키: keyboard025-test.log 및 graphics025-ralt-on.log 검사.', '오른쪽 Alt: keyboard025-test.log 및 옵션 ON/OFF RDP 입력 검사.')
(w/'package025.py').write_text(s,encoding='utf-8')

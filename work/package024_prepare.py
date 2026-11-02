from pathlib import Path
w=Path.cwd()/'work'
head='''WinHomeDesk 0.2.4 — 한영 전환키 처리 / 프로그램 이름 변경

업데이트
1. 기존 Surface Desk를 트레이 메뉴의 '종료 (서버도 종료)'로 완전히 종료합니다.
2. 새 ZIP 전체를 별도 폴더에 풀고 WinHomeDesk.exe를 실행합니다.
3. 기존 주소·포트·계정(surface)과 RDP/RDG 파일을 그대로 사용해 접속합니다.
4. 원격 서피스에서 한국어 Microsoft IME를 선택하고 메모장 등의 입력란을 클릭해 한영키를 시험하세요.
5. 자동 시작을 사용했다면 새 프로그램의 환경 설정에서 자동 시작을 저장하세요.
   새 실행 경로를 WinHomeDesk 이름으로 등록하고 이전 SurfaceDesk 자동 시작 항목을 정리합니다.
   업데이트 자체는 사용자의 자동 시작 설정을 변경하지 않습니다.

변경 내용
- 창 제목, 트레이, 알림, 환경 설정, 실행 파일, 기본 RDP/RDG/로그 파일명을 WinHomeDesk로 변경했습니다.
- 기존 암호 복호화, 설정, TLS 인증서와 중복 실행 방지를 위해 내부 저장 위치/식별자는 유지합니다.
  내부 코드와 엔진 진단 프로토콜에 SurfaceDesk 이름이 남을 수 있습니다.
- 한영·한자키의 71/72 및 F1/F2 스캔 코드와 키를 뗀 이벤트만 오는 경우를 처리합니다.
- 해당 키를 Windows IME 가상 키의 누름/뗌 한 쌍으로 전달합니다. 반복 입력과 짝이 되는 뗌은 중복 전환하지 않습니다.
- 오른쪽 Alt/Ctrl은 원격 전경 창의 한국어 키보드 배치가 한영/한자로 매핑한 경우만 변환합니다.
  다른 언어의 AltGr 및 일반 키는 기존 스캔 코드 전송을 유지합니다.
- 특별키 진단은 INPUT_IME에 남기며 일반 입력 문자나 암호는 기록하지 않습니다.

검증 범위
- 일반/특수/뗌 전용 스캔 코드, 반복 억제, 일반 키와 AltGr 보존 검사.
- RDP 테스트 클라이언트에서 엔진으로 전달한 한영 이벤트의 SendInput 결과 확인.
- GPU AVC420, Microsoft RDP 영상 연결, 설정 암호화 및 트레이 종료/복원 회귀 검사.
- 개발 실행 데스크톱에서는 키보드 훅 이벤트를 관찰하지 못했습니다.
  따라서 실제 원격 Surface의 한국어 IME 전환·한글 조합 및 물리 키보드는 아직 검증하지 못했습니다.
- 원격 창에 포커스가 없거나 클라이언트가 한영키를 로컬에서 처리하면 서버에 도착하지 않을 수 있습니다.
  계속 안 되면 한영키를 몇 번 누른 직후 로그를 저장해 INPUT_IME 발생 여부를 확인하세요.

이전 버전 기능 및 오디오 안내 (0.2.3의 검증 결과)
'''
old=(w/'사용안내023.txt').read_text(encoding='utf-8').replace('Surface Desk','WinHomeDesk').replace('SurfaceDesk.exe','WinHomeDesk.exe')
(w/'사용안내024.txt').write_text(head+old,encoding='utf-8')
s=(w/'package023.py').read_text(encoding='utf-8')
s=s.replace("pkg=w/'package/SurfaceDesk-0.2.3'", "pkg=w/'package/WinHomeDesk-0.2.4'")
s=s.replace("engine/name)", "engine/('WinHomeDesk-engine.exe' if name=='surface-rdp-engine.exe' else name))")
s=s.replace("for name in ['SurfaceDesk.exe','SurfaceDesk.ico']:shutil.copy2(w/name,pkg/name)", "shutil.copy2(w/'WinHomeDesk.exe',pkg/'WinHomeDesk.exe')\nshutil.copy2(w/'SurfaceDesk.ico',pkg/'WinHomeDesk.ico')")
s=s.replace("shutil.copy2(p,pkg/p.name)", "(pkg/p.name.replace('SurfaceDesk','WinHomeDesk')).write_bytes(p.read_bytes().replace(b'Surface Desk',b'WinHomeDesk'))")
s=s.replace("engine/'surface-rdp-engine.exe'", "engine/'WinHomeDesk-engine.exe'")
s=s.replace('SurfaceDesk-0.2.3-Windows-x64','WinHomeDesk-0.2.4-Windows-x64').replace('SurfaceDesk-0.2.3-Source','WinHomeDesk-0.2.4-Source')
s=s.replace("out/'SurfaceDesk-사용안내.txt'", "out/'WinHomeDesk-사용안내.txt'")
s=s.replace('사용안내023.txt','사용안내024.txt').replace('package023-test','package024-test')
s=s.replace('validation="""Surface Desk 0.2.3 검증 기록', 'validation="""WinHomeDesk 0.2.4 검증 기록\\n한영키: keyboard024-test.log 및 graphics024-input.log 검사. 실제 원격 IME 조합은 미검증.\\n설정·트레이 및 GPU/Microsoft RDP 영상 연결 회귀 통과.\\n아래 오디오 내용은 변경 없는 0.2.3 경로의 이전 검증 기록입니다.')
s=s.replace("(pkg/'검증결과.txt').write_text(validation", "validation+='\\n이번 버전 키보드 검사:\\n'+(w/'keyboard024-test.log').read_text(encoding='utf-8-sig')+'\\n'+(w/'gui024-tests.txt').read_text(encoding='utf-8')\nassert 'INPUT_IME key=hangul' in (w/'gfx020-avc420.log').read_text(errors='replace')\nassert 'PASS' in (w/'keyboard024-test.log').read_text(encoding='utf-8-sig')\n(pkg/'검증결과.txt').write_text(validation")
s=s.replace("names+=['test_pitch023.c'", "names+=['test_keyboard024.c','build_gui024.py','test_gui024.py','package024.py','package024_prepare.py','사용안내024.txt','test_pitch023.c'")
s=s.replace('SurfaceDesk-Source/', 'WinHomeDesk-Source/')
s=s.replace("s+='\\n0.2.3 additions:", "s+='\\n0.2.4: WinHomeDesk branding, Korean IME keyboard translation; backward-compatible storage/credentials.\\n0.2.3 additions:")
s=s.replace('Windows x64. LLVM-MinGW', 'WinHomeDesk.exe is the GUI. Rename the built shadow CLI to engine/WinHomeDesk-engine.exe when packaging. Internal SurfaceDesk names preserve existing settings and certificates.\nWindows x64. LLVM-MinGW')
(w/'package024.py').write_text(s,encoding='utf-8')

from pathlib import Path
import shutil,zipfile,json,hashlib
w=Path(__file__).resolve().parent;out=w.parent/'outputs';pkg=w/'package/WinHomeDesk-0.2.8'
old=w/'package/WinHomeDesk-0.2.7'
assert not pkg.exists(), 'Use a fresh package directory'
shutil.copytree(old,pkg)
shutil.copy2(w/'WinHomeDesk.exe',pkg/'WinHomeDesk.exe')
native=json.loads((old/'SHA256.json').read_text(encoding='utf-8'))
for p in (pkg/'engine').rglob('*'):
 if p.is_file():assert hashlib.sha256(p.read_bytes()).hexdigest()==native[str(p.relative_to(pkg))]
checks=(w/'client028-tests.txt').read_text(encoding='utf-8')
assert checks.count('PASS ')==11
assert (w/'ime-fix/WinHomeDesk.ImeHook.dll').read_bytes()==(out/'WinHomeDesk-RDCMan3.22-IME-1.0/WinHomeDesk.ImeHook.dll').read_bytes()
p=w/'Build.ps1';s=p.read_text(encoding='utf-8-sig')
s=s.replace("(Join-Path $base 'AppCredentials.cs')", "(Join-Path $base 'AppCredentials.cs') (Join-Path $base 'RdcManClient.cs') \"/resource:$(Join-Path $base 'ime-fix/WinHomeDesk.ImeHook.dll'),WinHomeDesk.ImeHook.dll\"")
p.write_text(s,encoding='utf-8-sig')
intro='''WinHomeDesk 0.2.8 — RDCMan 3.22 한글 입력 보정 통합

새 기능
- 메인 화면 상단 / 트레이 메뉴에 'RDCMan 실행 (한글 보정)' 추가.
- 작동 확인된 한글 보정 1.0 DLL을 WinHomeDesk.exe 안에 내장했습니다.
- 별도 WinHomeDesk-RDCMan-IME.exe를 실행할 필요가 없습니다.
- RDCMan 3.22의 새 RDCore 엔진을 그대로 사용합니다. -mstsc 전환 없음.

접속하는 PC에서 사용
1. ZIP 전체를 새 폴더에 풀고 WinHomeDesk.exe를 실행합니다.
2. 기존에 실행 중인 RDCMan은 먼저 종료합니다.
3. 'RDCMan 실행 (한글 보정)'을 누릅니다.
4. 처음 한 번 사용 중인 RDCMan 3.22의 RDCMan.exe를 선택합니다. 경로는 저장됩니다.
5. 열린 RDCMan에서 기존 RDG 파일로 접속합니다.
접속하는 PC에서는 화면 공유 동의나 서버 시작이 필요 없습니다.
이후에도 WinHomeDesk의 실행 버튼/트레이 메뉴로 RDCMan을 실행해야 보정이 적용됩니다.
RDCMan 자체는 별도 설치되어 있어야 합니다. Microsoft 실행 파일은 포함하지 않습니다.

환경 설정
- '경로 변경': 다른 RDCMan 3.22 실행 파일 선택 후 설정 저장.
- '한글 보정 로그 열기': 보정 적용 여부 확인.
- 보정 캐시와 로그: %LOCALAPPDATA%\\WinHomeDesk\\Client
  내장 모듈은 해시별 폴더에 자동으로 풀리므로 별도 DLL을 관리할 필요가 없습니다.
- 기존 서버 설정·암호 저장 방식은 유지됩니다.
- RDCMan은 별도 프로세스로 실행되며, WinHomeDesk를 종료해도 RDCMan은 강제 종료하지 않습니다.

접속받는 서피스
서버 엔진은 검증된 0.2.7과 동일합니다. 이미 0.2.7을 사용하는 서피스는
접속용 PC의 통합 기능을 위해 반드시 교체할 필요는 없습니다.
서버와 클라이언트 기능을 한 PC에서 함께 사용하는 것도 가능합니다.

아래는 기존 서버 기능 안내입니다.
------------------------------------------------------------
'''
guide=intro+(w/'사용안내027.txt').read_text(encoding='utf-8')
(w/'사용안내028.txt').write_text(guide,encoding='utf-8')
(pkg/'사용안내.txt').write_text(guide,encoding='utf-8-sig');(out/'WinHomeDesk-사용안내.txt').write_text(guide,encoding='utf-8-sig')
validation='''WinHomeDesk 0.2.8 통합 검증
- 사용자에게 작동 확인된 IME 보정 1.0 모듈과 내장 DLL 바이트 일치.
- 내장 모듈 추출/해시 확인/변조된 시험 캐시 복원/정상 캐시 재사용 확인.
- 내장 모듈로 실제 원본 RDCMan 3.22 실행, STA UI 스레드에 보정 활성화 확인.
- 새 RDCore 엔진 유지, 자식 프로세스에만 보정 환경변수 적용, 기존 환경변수 유지.
- 메인 버튼은 서버 중지/실행 상태 모두 사용 가능. 경로 저장 및 기존 설정 호환.
- 트레이 숨김/복원/서버 종료, 암호화 설정 저장, 성능 표시 회귀 통과.
- 메인 화면/환경 설정 렌더링 후 배치 확인.
- 서버 엔진의 모든 파일은 0.2.7 SHA256과 일치. 서버 프로토콜 변경 없음.
- 실제 한글 입력은 직전 보정 도구에 대한 사용자의 성공 확인을 근거로 함.
  통합본에서 실제 원격 한글 입력을 다시 자동 재현한 것은 아님.

'''+checks+'\n기존 서버 0.2.7 검증 기록 (이 버전에서 재시험한 것으로 해석하지 마세요):\n'+(old/'검증결과.txt').read_text(encoding='utf-8-sig')
(pkg/'검증결과.txt').write_text(validation,encoding='utf-8-sig')
p=pkg/'licenses/NOTICE.txt';p.write_text(p.read_text(encoding='utf-8-sig')+'\n0.2.8: integrated process-local RDCMan 3.22 IME startup hook, embedded in WinHomeDesk.exe. The custom hook is Apache-2.0. Original RDCMan binaries are not redistributed or modified. Server binaries identical to 0.2.7.\n',encoding='utf-8')
manifest={str(p.relative_to(pkg)):hashlib.sha256(p.read_bytes()).hexdigest() for p in pkg.rglob('*') if p.is_file() and p.name!='SHA256.json'}
(pkg/'SHA256.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf-8')
archive=out/'WinHomeDesk-0.2.8-Windows-x64.zip'
with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED,6) as z:
 for p in pkg.rglob('*'):
  if p.is_file():z.write(p,p.relative_to(pkg.parent))
replacements=['SurfaceDesk.cs','SurfaceDesk.Lifecycle.cs','DesktopOptions.cs','Build.ps1','TestDesktopOptions.cs','TestLifecycle017.cs']
added=['RdcManClient.cs','build_gui028.py','test_client028.py','TestClient028.cs','package028.py','사용안내028.txt','ime-fix/StartupHook.cs','ime-fix/WinHomeDesk.ImeHook.dll','ime-fix/build.py','ime-fix/build.ps1']
source=out/'WinHomeDesk-0.2.8-Source.zip'
with zipfile.ZipFile(out/'WinHomeDesk-0.2.7-Source.zip') as before,zipfile.ZipFile(source,'w',zipfile.ZIP_DEFLATED,6) as z:
 for info in before.infolist():
  rel=info.filename.removeprefix('WinHomeDesk-Source/')
  if rel in replacements or rel=='BUILD-INFO.txt' or rel=='licenses/NOTICE.txt':continue
  z.writestr(info,before.read(info))
 for name in replacements+added:z.write(w/name,'WinHomeDesk-Source/'+name)
 z.write(pkg/'licenses/NOTICE.txt','WinHomeDesk-Source/licenses/NOTICE.txt')
 z.writestr('WinHomeDesk-Source/BUILD-INFO.txt',before.read('WinHomeDesk-Source/BUILD-INFO.txt').decode().replace('build_gui027.py','build_gui028.py')+'\n0.2.8 adds RdcManClient.cs and embeds ime-fix/WinHomeDesk.ImeHook.dll. The included prebuilt custom hook is the user-confirmed 1.0 module; source and build scripts included. Rebuilding the hook requires PowerShell 7/.NET 10 and WindowsDesktop reference assemblies (adjust path in ime-fix/build.ps1). Main GUI still targets .NET Framework 4.x. Server sources/binaries unchanged from 0.2.7.\n')
for path in [archive,source]:
 with zipfile.ZipFile(path) as z:assert z.testzip() is None
 print(path.name,path.stat().st_size,hashlib.sha256(path.read_bytes()).hexdigest(),flush=True)

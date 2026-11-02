from pathlib import Path
import shutil,zipfile,hashlib,json,subprocess,os,socket,time,ast,statistics
r=Path.cwd();w=r/'work';out=r/'outputs';pkg=w/'package/SurfaceDesk-0.2.1';pkg.mkdir(parents=True,exist_ok=True)
engine=pkg/'engine';engine.mkdir(exist_ok=True)
dlls=['libfreerdp-server3.dll','libfreerdp-shadow-subsystem3.dll','libfreerdp-shadow3.dll','libfreerdp3.dll','libssl-3-x64.dll','libcrypto-3-x64.dll','libwinpr-tools3.dll','libwinpr3.dll','zlib1.dll','libopenh264-8.dll','libgcc_s_seh-1.dll','libstdc++-6.dll','libwinpthread-1.dll']
for name in dlls+['surface-rdp-engine.exe']:shutil.copy2(w/'runtime'/name,engine/name)
(engine/'ossl-modules').mkdir(exist_ok=True);shutil.copy2(w/'deps/ucrt64/lib/ossl-modules/legacy.dll',engine/'ossl-modules/legacy.dll')
for name in ['SurfaceDesk.exe','SurfaceDesk.ico']:shutil.copy2(w/name,pkg/name)
for p in [out/'SurfaceDesk-내서피스.rdg',out/'SurfaceDesk-192.168.0.110-3390.rdp']:shutil.copy2(p,pkg/p.name)
lic=pkg/'licenses';lic.mkdir(exist_ok=True)
for p in (w/'package/SurfaceDesk-0.2.0/licenses').iterdir():shutil.copy2(p,lic/p.name)
p=lic/'NOTICE.txt';s=p.read_text(encoding='utf-8-sig').replace('Surface Desk 0.2.0','Surface Desk 0.2.1')
s+='\n0.2.1 additions: H.264 worker with input pumping on the transport owner thread; Windows Media Foundation hardware AVC420 encoder with CPU fallback, high-resolution result waits, stream-change and parameter-set handling. Uses Windows mf/mfplat/mfuuid/strmiids/ole32/oleaut32 system APIs. No Windows system DLLs are redistributed.\n'
p.write_text(s,encoding='utf-8')
auth=json.loads((w/'auth021-results.json').read_text(encoding='utf-8-sig'));gfx=json.loads((w/'graphics021-results.json').read_text(encoding='utf-8-sig'));audio=json.loads((w/'audio021-results.json').read_text(encoding='utf-8-sig'));bench=json.loads((w/'bench021-results.json').read_text(encoding='utf-8-sig'))
assert len(gfx)==11 and len(audio)==4 and auth['wrong_password']['exit']==1 and auth['correct_password']['exit']==0
assert (w/'validation021-results.json').exists() and (w/'lifecycle021-results.json').exists()
assert 'backend=gpu' in (w/'gpu021-server.log').read_text(encoding='utf-8-sig') and 'during_encode=' in (w/'cpu021-server.log').read_text(encoding='utf-8-sig')
# Exercise the exact packaged DLLs with only the system DLL path available.
secret='package-'+os.urandom(16).hex();salt=os.urandom(16);env=os.environ.copy()
env.update(PATH=os.environ['WINDIR']+'\\System32',SURFACE_DESK_SALT=salt.hex(),SURFACE_DESK_HASH=hashlib.pbkdf2_hmac('sha1',secret.encode(),salt,200000,32).hex(),SURFACE_DESK_DATA=str(w/'package021-test-state'),OPENSSL_MODULES=str(engine/'ossl-modules'),SURFACE_DESK_CAPTURE='gdi',SURFACE_DESK_MAX_EDGE='1440',SURFACE_DESK_GPU='1',SURFACE_DESK_AUDIO='1',SURFACE_PROBE_GFX='420',SURFACE_PROBE_AUDIO='1',SURFACE_PROBE_AUDIO_STATS='1',SURFACE_PROBE_DURATION_MS='4000')
with (w/'package021-test.log').open('w') as log:
 server=subprocess.Popen([str(engine/'surface-rdp-engine.exe'),'/bind-address:127.0.0.1','/port:13390','/sec:tls','+auth','+may-view','+may-interact','/max-connections:1','+gfx','+gfx-avc420','-gfx-avc444','+gfx-planar','-gfx-progressive','-rfx','-nsc','+bitmap-compat'],env=env,cwd=engine,stdout=log,stderr=subprocess.STDOUT)
 try:
  for _ in range(100):
   assert server.poll() is None,'packaged engine exited'
   try:
    with socket.create_connection(('127.0.0.1',13390),timeout=.2):break
   except OSError:time.sleep(.1)
  else:raise RuntimeError('listener timeout')
  p=subprocess.run([str(w/'runtime/surface-probe.exe'),secret],env=env,cwd=w/'runtime',capture_output=True,timeout=20)
  text=p.stdout.decode(errors='replace')+p.stderr.decode(errors='replace');assert p.returncode==0 and 'codec=11' in text,text[-1600:]
 finally:
  if server.poll() is None:server.terminate();server.wait(timeout=5)
text=(w/'package021-test.log').read_text(errors='replace');assert 'backend=gpu' in text and 'AUDIO_CHANNEL state=ready' in text
def mean(mode,key):return statistics.mean(a[key] for a in bench if a['mode']==mode)
comparison=f'''개발 PC 합성 영상 비교 (1440x960, 움직임 90프레임 × 각 3회)
- CPU/GPU 평균 압축·준비: {mean('cpu','mean_ms'):.2f}ms / {mean('gpu','mean_ms'):.2f}ms.
- 각 실행 중앙값의 평균: CPU {mean('cpu','p50_ms'):.2f}ms / GPU {mean('gpu','p50_ms'):.2f}ms.
- 첫 인코더 준비가 포함된 최대 시간: CPU 약 {mean('cpu','max_ms'):.0f}ms / GPU 약 {mean('gpu','max_ms'):.0f}ms.
- GPU 압축 데이터량은 이 장면에서 CPU 대비 약 {mean('gpu','bytes')/mean('cpu','bytes'):.1f}배. 동일 화질·압축률 비교가 아닙니다.
- CPU 사용률, 실제 Surface/LAN의 FPS 및 왕복 지연은 측정하지 않았습니다. 성능 향상을 보장하지 않습니다.
'''
guide=(w/'사용안내021.txt').read_text(encoding='utf-8')+'\n'+comparison
(pkg/'사용안내.txt').write_text(guide,encoding='utf-8-sig');(out/'SurfaceDesk-사용안내.txt').write_text(guide,encoding='utf-8-sig')
validation='''Surface Desk 0.2.1 검증 기록
- Intel Quick Sync Video H.264 Encoder MFT를 실제 선택해 AVC420 인코딩 및 FreeRDP 복호화 성공.
- GPU/CPU 각각 합성 움직임 320x180 및 1440x960 색상 오차·패딩·부분 영역 검사 통과.
- GPU 초기화 실패를 COM apartment 충돌로 유도: CPU 자동 전환 후 복호화 성공.
- 실제 RDP 루프백에서 H.264 압축 중 마우스 입력 SendInput 성공을 서버 카운터로 확인 (CPU/GPU).
- 450ms 느린 작업 동안 소유 스레드 응답, 동시 두 작업 거절, 동일 스레드 정리, 완료 후 해제 검사.
- GPU 초기 연결 중 빠른 종료 2회 후 재접속, 화면 중지/재개, 45FPS 설정의 GPU 접속 검사 통과.
- Microsoft RDP ActiveX 32비트 AVC420/AVC444v2 연결 유지, 16비트 비트맵 대체 검사.
- AVC420/AVC444/v1/v2, Planar, 비트맵 및 기능 협상에 따른 대체 등 11개 접속 검사.
- WASAPI → rdpsnd → 테스트 클라이언트 PCM 비무음 수신; 서버/클라이언트 소리 끄기 및 인증 오류에서 미전송 검사.
- 정확한 암호/계정만 영상 허용. 누락/오류 암호·잘못된 사용자 차단.
- GPU 설정 저장, 구버전 설정 기본값, 실행 중 변경 금지, 트레이·종료·DPAPI 검사.
- RDP 다중 가져오기/RDG 생성, 설치된 RDCMan XML 파서의 색상 32비트 및 오디오 설정 수용 확인.
- 배포 DLL 그대로 Windows System32만 포함한 PATH에서 GPU AVC420+오디오 채널 접속 성공.
- 실제 자동 시작 레지스트리는 변경하지 않음. 실제 원격 Surface, DXGI+GPU 전체 경로, 재부팅, 원격 스피커 청취는 미검증.
- 개발 환경 접속 시험은 GDI 캡처. 실제 사용자 화면 내용/암호/개인 로그는 이 ZIP에 포함하지 않음.
- 입력 분리는 H.264에 적용. 네트워크 전송 차단/클라이언트 화면 표시 지연은 별도 병목으로 남음.
- GPU는 CPU 메모리 업로드를 포함하며 완전한 GPU 무복사 구현은 아님.
\n'''+comparison+'\n'+(w/'worker021.log').read_text(encoding='utf-8-sig')+'\n'+(w/'gui021.log').read_text(encoding='utf-8-sig')+'\n'+(w/'rdg021.log').read_text(encoding='utf-8-sig')
(pkg/'검증결과.txt').write_text(validation,encoding='utf-8-sig')
for src,dst in [('bench021-results.json','GPU성능비교-개발PC.json'),('graphics021-results.json','그래픽검증-개발PC.json'),('audio021-results.json','오디오검증-개발PC.json'),('lifecycle021-results.json','접속수명검증.json')]:shutil.copy2(w/src,pkg/dst)
manifest={str(p.relative_to(pkg)):hashlib.sha256(p.read_bytes()).hexdigest() for p in pkg.rglob('*') if p.is_file() and p.name!='SHA256.json'}
(pkg/'SHA256.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf-8')
with zipfile.ZipFile(out/'SurfaceDesk-0.2.1-Windows-x64.zip','w',zipfile.ZIP_DEFLATED,6) as z:
 for p in pkg.rglob('*'):
  if p.is_file():z.write(p,str(p.relative_to(pkg.parent)))
old=(w/'package020.py').read_text(encoding='utf-8-sig');names=ast.literal_eval(next(a.strip()[6:] for a in old.splitlines() if a.strip().startswith('names=[')))
names+=['EncoderMode.cs','test_worker021.c','build_gui021.py','validate021.py','test_lifecycle021.py','package021.py','guide021.py','사용안내020.txt','사용안내021.txt','package020.py']
with zipfile.ZipFile(out/'SurfaceDesk-0.2.1-Source.zip','w',zipfile.ZIP_DEFLATED,6) as z:
 for p in (w/'FreeRDP').rglob('*'):
  rel=p.relative_to(w/'FreeRDP')
  if '.git' not in rel.parts and p.is_file():z.write(p,'SurfaceDesk-Source/FreeRDP/'+rel.as_posix())
 for name in sorted(set(names)):z.write(w/name,'SurfaceDesk-Source/'+name)
 for p in lic.iterdir():z.write(p,'SurfaceDesk-Source/licenses/'+p.name)
 z.writestr('SurfaceDesk-Source/FreeRDP/.source_version','dae0390a42ab271fa07a35979dfcd5671f83a949\n')
 z.writestr('SurfaceDesk-Source/FreeRDP/.source_tag','3.32.3-dev0\n')
 z.writestr('SurfaceDesk-Source/BUILD-INFO.txt','''Windows x64. LLVM-MinGW 20261020 UCRT x64 (C and C++), CMake 4.4.4, Ninja 1.13.2, .NET Framework 4.x csc.
DependencyRoot: MSYS2 UCRT64 OpenSSL 3.6.5, zlib-ng-compat 2.3.3, OpenH264 2.7.0 headers/import libs.
Build.ps1 -CompilerRoot <llvm-mingw-root> -DependencyRoot <ucrt64-root> -CMake <cmake.exe> -Ninja <ninja.exe>
New GUI source EncoderMode.cs must be compiled with the other GUI sources (Build.ps1 includes it).
SURFACE_DESK_GPU=1 requests custom Windows Media Foundation hardware AVC420. Failure falls back to OpenH264. AVC444 remains OpenH264.
The upstream WITH_MEDIA_FOUNDATION option is OFF; custom surface_mf.cpp is independently compiled in this Windows fork.
Windows system MF/COM DLLs must not be copied into the package. No external GPU encoder DLL is required.
H.264 encoder creation/use/destruction is serialized on a per-client worker; all peer transport reads/writes stay on its original I/O thread.
SURFACE_DESK_AUDIO=1 enables WASAPI loopback. Audio/input test probes are excluded from the runtime package.
Diagnostic scripts assume the development work/build-ready and work/runtime layout. Adapt paths as necessary; no production credentials are needed.
Source package contains all CMake test target source files. The tests compile/run separately from the runtime GUI.
See licenses/NOTICE.txt and dependency-manifest.json for dependency licenses and pinned package hashes.
''')
for name in ['SurfaceDesk-0.2.1-Windows-x64.zip','SurfaceDesk-0.2.1-Source.zip']:
 with zipfile.ZipFile(out/name) as z:assert z.testzip() is None
 print(name,(out/name).stat().st_size,hashlib.sha256((out/name).read_bytes()).hexdigest(),flush=True)
print('Packaged GPU AVC420/audio verified with clean PATH. Archives verified.',flush=True)

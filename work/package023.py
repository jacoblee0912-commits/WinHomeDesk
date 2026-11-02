from pathlib import Path
import shutil,zipfile,hashlib,json,subprocess,os,socket,time,ast,statistics
r=Path.cwd();w=r/'work';out=r/'outputs';pkg=w/'package/SurfaceDesk-0.2.3';pkg.mkdir(parents=True,exist_ok=True)
engine=pkg/'engine';engine.mkdir(exist_ok=True)
dlls=['libfreerdp-server3.dll','libfreerdp-shadow-subsystem3.dll','libfreerdp-shadow3.dll','libfreerdp3.dll','libssl-3-x64.dll','libcrypto-3-x64.dll','libwinpr-tools3.dll','libwinpr3.dll','zlib1.dll','libopenh264-8.dll','libgcc_s_seh-1.dll','libstdc++-6.dll','libwinpthread-1.dll']
for name in dlls+['surface-rdp-engine.exe']:shutil.copy2(w/'runtime'/name,engine/name)
(engine/'ossl-modules').mkdir(exist_ok=True);shutil.copy2(w/'deps/ucrt64/lib/ossl-modules/legacy.dll',engine/'ossl-modules/legacy.dll')
for name in ['SurfaceDesk.exe','SurfaceDesk.ico']:shutil.copy2(w/name,pkg/name)
for p in [out/'SurfaceDesk-내서피스.rdg',out/'SurfaceDesk-192.168.0.110-3390.rdp']:shutil.copy2(p,pkg/p.name)
lic=pkg/'licenses';lic.mkdir(exist_ok=True)
for p in (w/'package/SurfaceDesk-0.2.0/licenses').iterdir():shutil.copy2(p,lic/p.name)
p=lic/'NOTICE.txt';s=p.read_text(encoding='utf-8-sig').replace('Surface Desk 0.2.0','Surface Desk 0.2.3')
s+='\n0.2.1 additions: H.264 worker with input pumping on the transport owner thread; Windows Media Foundation hardware AVC420 encoder with CPU fallback, high-resolution result waits, stream-change and parameter-set handling. Uses Windows mf/mfplat/mfuuid/strmiids/ole32/oleaut32 system APIs. No Windows system DLLs are redistributed.\n'
s+='\n0.2.3 additions: 44.1kHz PCM capture/packet/negotiation compatibility, actual Microsoft playback regression; 10ms PCM chunks, WASAPI buffer/poll tuning, full-width Wave2 capture timestamps, duplicate-aware audio confirmation diagnostics.\n'
p.write_text(s,encoding='utf-8')
auth=json.loads((w/'auth021-results.json').read_text(encoding='utf-8-sig'));gfx=json.loads((w/'graphics021-results.json').read_text(encoding='utf-8-sig'));audio=json.loads((w/'audio023-results.json').read_text(encoding='utf-8-sig'));bench=json.loads((w/'bench021-results.json').read_text(encoding='utf-8-sig'))
assert 'SURFACE_AUDIO_TEST_SOURCE:BOOL=OFF' in (w/'build-ready/CMakeCache.txt').read_text()
assert len(gfx)==11 and len(audio)==6 and auth['wrong_password']['exit']==1 and auth['correct_password']['exit']==0
assert (w/'audio023-integration.log').exists() and (w/'audio022-timing.log').exists()
assert 'backend=gpu' in (w/'gfx020-avc420.log').read_text(encoding='utf-8-sig',errors='replace')
# Exercise the exact packaged DLLs with only the system DLL path available.
secret='package-'+os.urandom(16).hex();salt=os.urandom(16);env=os.environ.copy()
env.update(PATH=os.environ['WINDIR']+'\\System32',SURFACE_DESK_SALT=salt.hex(),SURFACE_DESK_HASH=hashlib.pbkdf2_hmac('sha1',secret.encode(),salt,200000,32).hex(),SURFACE_DESK_DATA=str(w/'package023-test-state'),OPENSSL_MODULES=str(engine/'ossl-modules'),SURFACE_DESK_CAPTURE='gdi',SURFACE_DESK_MAX_EDGE='1440',SURFACE_DESK_GPU='1',SURFACE_DESK_AUDIO='1',SURFACE_PROBE_GFX='420',SURFACE_PROBE_AUDIO='1',SURFACE_PROBE_AUDIO_STATS='1',SURFACE_PROBE_DURATION_MS='4000')
with (w/'package023-test.log').open('w') as log:
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
text=(w/'package023-test.log').read_text(errors='replace');assert 'backend=gpu' in text and 'AUDIO_CHANNEL state=ready' in text
assert 'AUDIO_TEST' not in text and 'rate=44100' in text, text[-2000:]

def mean(mode,key):return statistics.mean(a[key] for a in bench if a['mode']==mode)
comparison=f'''0.2.1에서 수행한 개발 PC 합성 영상 비교 (1440x960, 움직임 90프레임 × 각 3회)
- CPU/GPU 평균 압축·준비: {mean('cpu','mean_ms'):.2f}ms / {mean('gpu','mean_ms'):.2f}ms.
- 각 실행 중앙값의 평균: CPU {mean('cpu','p50_ms'):.2f}ms / GPU {mean('gpu','p50_ms'):.2f}ms.
- 첫 인코더 준비가 포함된 최대 시간: CPU 약 {mean('cpu','max_ms'):.0f}ms / GPU 약 {mean('gpu','max_ms'):.0f}ms.
- GPU 압축 데이터량은 이 장면에서 CPU 대비 약 {mean('gpu','bytes')/mean('cpu','bytes'):.1f}배. 동일 화질·압축률 비교가 아닙니다.
- CPU 사용률, 실제 Surface/LAN의 FPS 및 왕복 지연은 측정하지 않았습니다. 성능 향상을 보장하지 않습니다.
'''
guide=(w/'사용안내023.txt').read_text(encoding='utf-8')+'\n'+comparison
(pkg/'사용안내.txt').write_text(guide,encoding='utf-8-sig');(out/'SurfaceDesk-사용안내.txt').write_text(guide,encoding='utf-8-sig')
validation="""Surface Desk 0.2.3 검증 기록
- WASAPI 실제 캡처 및 44.1kHz 스테레오 PCM 10ms(1764바이트) 패킷 수신 확인.
- Wave2의 32비트 캡처 시각이 잘리지 않는지 루프백 시각 차이로 확인.
- 구버전 Wave 프로토콜에서도 동일한 10ms PCM 수신 확인.
- 테스트 클라이언트가 350ms 재생 지연을 보고하도록 설정해 서버 계측에서 감지하는지 확인.
  이는 실제 스피커 지연을 350ms로 만든 시험이 아닙니다.
- 두 번 회신하는 ACK, 16비트 타임스탬프 순환, 중복·누락·미일치 회신, 블록 번호 재사용 단위 검사.
- 서버/클라이언트 소리 끄기 및 틀린 암호에서 PCM 미수신 검사.
- GPU AVC420 및 Microsoft RDP ActiveX 영상 연결 회귀 검사.
- 새 오디오 계측 UI의 확인 대기/지연 표시/잘못된 값 처리 검사 및 트레이·설정 회귀 검사.
- 완성 배포 DLL 그대로 제한된 PATH에서 GPU AVC420 및 오디오 채널 연결 확인.
- 일반 회귀 검사는 GDI 캡처 및 재생 없는 테스트 오디오 수신기를 사용합니다.
- 별도 시험용 빌드의 440Hz 생성기로 실제 Microsoft RDP ActiveX 재생을 확인했습니다.
  48kHz 경로는 최대 보고 지연이 약 15초 동안 1.2초 이상으로 증가했고,
  44.1kHz 경로는 약 18초 시험에서 평균 약 125ms / 최대 약 141ms로 안정됐습니다.
  실제 재생된 시험음의 WASAPI 측정 결과 440.018Hz를 확인했습니다.
- 배포본의 시험 생성기 OFF 및 실제 WASAPI 44.1kHz 캡처를 확인했습니다.
- 실제 원격 Surface/RDCMan 청취 지연 및 블루투스 장치 지연은 이번 환경에서 측정하지 못했습니다.
- 함께 실린 전체 그래픽·GPU 성능·접속 수명 비교 JSON은 0.2.1에서 수행한 이전 검증 자료입니다.
\n"""+(w/'audio022-timing.log').read_text(encoding='utf-8')+'\n'+(w/'audio022-ui.log').read_text(encoding='utf-8')+'\n'+(w/'audio023-integration.log').read_text(encoding='utf-8')
(pkg/'검증결과.txt').write_text(validation,encoding='utf-8-sig')
for src,dst in [('bench021-results.json','GPU성능비교-개발PC.json'),('graphics021-results.json','그래픽검증-개발PC.json'),('audio023-results.json','오디오검증-개발PC.json'),('lifecycle021-results.json','접속수명검증.json')]:shutil.copy2(w/src,pkg/dst)
manifest={str(p.relative_to(pkg)):hashlib.sha256(p.read_bytes()).hexdigest() for p in pkg.rglob('*') if p.is_file() and p.name!='SHA256.json'}
(pkg/'SHA256.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf-8')
with zipfile.ZipFile(out/'SurfaceDesk-0.2.3-Windows-x64.zip','w',zipfile.ZIP_DEFLATED,6) as z:
 for p in pkg.rglob('*'):
  if p.is_file():z.write(p,str(p.relative_to(pkg.parent)))
old=(w/'package020.py').read_text(encoding='utf-8-sig');names=ast.literal_eval(next(a.strip()[6:] for a in old.splitlines() if a.strip().startswith('names=[')))
names+=['test_pitch023.c','test_audio023.py','validate023.py','build_gui023.py','package023.py','package023_prepare.py','사용안내023.txt','test_audio_timing022.c','test_AudioCaption022.cs','test_audio022.py','validate022.py','build_gui022.py','package022.py','package022_prepare.py','audio022_testgen.py','EncoderMode.cs','test_worker021.c','build_gui021.py','validate021.py','test_lifecycle021.py','package021.py','guide021.py','사용안내020.txt','사용안내023.txt','package020.py']
with zipfile.ZipFile(out/'SurfaceDesk-0.2.3-Source.zip','w',zipfile.ZIP_DEFLATED,6) as z:
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
Production builds force SURFACE_AUDIO_TEST_SOURCE=OFF. Audio uses 44.1kHz stereo 16-bit, 10ms PCM with Wave2 capture timestamps and WaveConfirm diagnostic estimates.
Source package contains all CMake test target source files. The tests compile/run separately from the runtime GUI.
See licenses/NOTICE.txt and dependency-manifest.json for dependency licenses and pinned package hashes.
''')
for name in ['SurfaceDesk-0.2.3-Windows-x64.zip','SurfaceDesk-0.2.3-Source.zip']:
 with zipfile.ZipFile(out/name) as z:assert z.testzip() is None
 print(name,(out/name).stat().st_size,hashlib.sha256((out/name).read_bytes()).hexdigest(),flush=True)
print('Packaged GPU AVC420/audio verified with clean PATH. Archives verified.',flush=True)

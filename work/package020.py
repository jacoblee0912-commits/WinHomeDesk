from pathlib import Path
import shutil,zipfile,hashlib,json,subprocess,os,socket,time
r=Path.cwd();w=r/'work';out=r/'outputs';pkg=w/'package/SurfaceDesk-0.2.0';pkg.mkdir(parents=True,exist_ok=True)
engine=pkg/'engine';engine.mkdir(exist_ok=True)
dlls=['libfreerdp-server3.dll','libfreerdp-shadow-subsystem3.dll','libfreerdp-shadow3.dll','libfreerdp3.dll','libssl-3-x64.dll','libcrypto-3-x64.dll','libwinpr-tools3.dll','libwinpr3.dll','zlib1.dll','libopenh264-8.dll','libgcc_s_seh-1.dll','libstdc++-6.dll','libwinpthread-1.dll']
for name in dlls:shutil.copy2(w/'runtime'/name,engine/name)
shutil.copy2(w/'runtime/surface-rdp-engine.exe',engine/'surface-rdp-engine.exe')
(engine/'ossl-modules').mkdir(exist_ok=True);shutil.copy2(w/'deps/ucrt64/lib/ossl-modules/legacy.dll',engine/'ossl-modules/legacy.dll')
for name in ['SurfaceDesk.exe','SurfaceDesk.ico']:shutil.copy2(w/name,pkg/name)
guide=(w/'사용안내020.txt').read_text(encoding='utf-8')
(pkg/'사용안내.txt').write_text(guide,encoding='utf-8-sig');(out/'SurfaceDesk-사용안내.txt').write_text(guide,encoding='utf-8-sig')
lic=pkg/'licenses';lic.mkdir(exist_ok=True)
for p in (w/'package/SurfaceDesk-0.1.7/licenses').glob('*'):
 if p.is_file() and p.name!='NOTICE.txt':shutil.copy2(p,lic/p.name)
for dep in ['openh264','libgcc','libstdc++','libwinpthread']:
 for p in (w/'deps/ucrt64/share/licenses'/dep).glob('*'):
  if p.is_file():shutil.copy2(p,lic/(dep+'-'+p.name))
notice='''Surface Desk 0.2.0 custom prototype. Custom files and original icon licensed under Apache-2.0.
Based on FreeRDP 3.32.3-dev0, commit dae0390a42ab271fa07a35979dfcd5671f83a949.
Upstream copyright and license headers retained. https://github.com/FreeRDP/FreeRDP
Modified Windows shadow subsystem (GDI/DXGI capture and GPU resizing), CLI startup/security/config path,
latest-frame mailbox and capture/delivery separation, high-resolution pacing, tuned OpenH264 CPU profile and stage metrics,
WASAPI loopback system audio, authenticated rdpsnd output, bounded audio queue,
compiler thread compatibility, graphics capability negotiation/color-depth fallback, diagnostic counters, test targets and GUI.
OpenH264 2.7.0 (BSD-2-Clause), software encoding: https://github.com/cisco/openh264/tree/v2.7.0
MSYS2 UCRT64 runtime packages: OpenSSL 3.6.5, zlib-ng-compat 2.3.3, OpenH264 2.7.0-1,
libgcc/libstdc++ 16.2.0-6 and libwinpthread 14.0.0.r569.g34eeafb7d-1. Package hashes in dependency-manifest.json.
Source/recipes: https://github.com/msys2/MINGW-packages/tree/master/mingw-w64-openh264
https://mirror.msys2.org/mingw/sources/mingw-w64-gcc-16.2.0-6.src.tar.zst
https://gcc.gnu.org/pub/gcc/releases/gcc-16.2.0/
https://mirror.msys2.org/mingw/sources/mingw-w64-winpthreads-14.0.0.r569.g34eeafb7d-1.src.tar.zst
Uses Windows Direct3D 11, DXGI and D3DCompiler; no Microsoft icon is copied.
No RDPWrap code/executable or patched Windows remote desktop service is included.
'''
(lic/'NOTICE.txt').write_text(notice,encoding='utf-8');shutil.copy2(w/'deps018-manifest.json',lic/'dependency-manifest.json')
auth=json.loads((w/'auth020-results.json').read_text());graphics=json.loads((w/'graphics020-results.json').read_text())
assert len(graphics)==11 and auth['correct_password']['exit']==0 and auth['wrong_password']['exit']==1
gui=(w/'gui020-tests.txt').read_text();assert 'Lifecycle:' in gui and 'PASS' in gui
codec=(w/'codec020-results.txt').read_text(encoding='utf-16' if (w/'codec020-results.txt').read_bytes()[:2]==b'\xff\xfe' else 'utf-8',errors='replace')
assert 'Moving codec mode=2 decoded=18/18' in codec and 'partial region tests PASS' in codec
# Test the packaged engine with a clean PATH to catch missing native runtime dependencies.
secret='package-'+os.urandom(16).hex();salt=os.urandom(16);env=os.environ.copy()
env.update(PATH=os.environ['WINDIR']+'\\System32',SURFACE_DESK_SALT=salt.hex(),SURFACE_DESK_HASH=hashlib.pbkdf2_hmac('sha1',secret.encode(),salt,200000,32).hex(),SURFACE_DESK_DATA=str(w/'package020-test-state'),OPENSSL_MODULES=str(engine/'ossl-modules'),SURFACE_DESK_CAPTURE='gdi',SURFACE_DESK_MAX_EDGE='1440',SURFACE_PROBE_GFX='444')
with (w/'package020-test.log').open('w') as log:
 env.update(SURFACE_DESK_AUDIO='1',SURFACE_PROBE_AUDIO='1',SURFACE_PROBE_AUDIO_STATS='1',SURFACE_PROBE_DURATION_MS='4000')
 server=subprocess.Popen([str(engine/'surface-rdp-engine.exe'),'/bind-address:127.0.0.1','/port:13390','/sec:tls','+auth','+may-view','+may-interact','/max-connections:1','+gfx','+gfx-avc420','+gfx-avc444','+gfx-planar','-gfx-progressive','-rfx','-nsc','+bitmap-compat'],env=env,cwd=engine,stdout=log,stderr=subprocess.STDOUT)
 try:
  for _ in range(100):
   assert server.poll() is None,'packaged engine failed'
   try:
    with socket.create_connection(('127.0.0.1',13390),timeout=.2):break
   except OSError:time.sleep(.1)
  else:raise RuntimeError('package listener timeout')
  probe=subprocess.run([str(w/'runtime/surface-probe.exe'),secret],cwd=w/'runtime',env=env,capture_output=True,timeout=20)
  result=probe.stdout.decode(errors='replace');assert probe.returncode==0 and 'codec=15' in result,result
 finally:
  if server.poll() is None:server.terminate();server.wait(timeout=5)
assert 'CODEC name=avc444v2' in (w/'package020-test.log').read_text(errors='replace')
assert 'AUDIO state=capturing' in (w/'package020-test.log').read_text(errors='replace')
assert 'AUDIO_CHANNEL state=ready' in (w/'package020-test.log').read_text(errors='replace')
audio=json.loads((w/'audio020-results.json').read_text());assert len(audio)==4 and audio[0]['bytes']>192000 and audio[0]['nonzero']>1000
pipeline=(w/'pipeline020-results.txt').read_text();pacer=(w/'pacer020-results.txt').read_text();bench=json.loads((w/'bench020-results.json').read_text());assert 'PASS:' in pipeline and 'PASS:' in pacer and len(bench)==6
validation='''Surface Desk 0.2.0 검증 기록
- 실제 프레임 버퍼 코드로 느린 소비자 시험: 캡처 진행, 처리 중 화면 불변, 마지막 화면 수신, 교체 수 보존 검사.
- 고해상도 타이머 20/30/45FPS 동작과 긴 대기 중 중지 이벤트 반응 검사.
- 1440x960 합성 움직임의 멀티스레드 AVC420/444/v2 인코딩·복호화·색상 오차 검사.
- 0.1.9 배포 DLL과 0.2.0의 같은 90프레임 작업을 3회 교차 실행한 비교 결과 별도 JSON 첨부.
- 실제 Surface에서의 FPS 개선 폭은 미검증. GPU 하드웨어 인코더는 적용하지 않음.
- WASAPI 기본 재생 장치 48kHz 스테레오 16비트 초기화 및 테스트음 캡처 성공.
- WASAPI → rdpsnd → FreeRDP 테스트 클라이언트로 실제 PCM 수신 및 비영(비무음) 샘플 확인.
- 서버 소리 끄기, 클라이언트 소리 끄기, 암호 오류에서 수신 PCM 0바이트 확인.
- 오디오 활성화 상태에서도 H.264 화면 연결 유지. 틀린 암호에서 오디오 채널을 열지 않음.
- 실제 RDCMan 설정 파서가 Client/High/DoNotRecord 설정을 수용.
- 오디오 옵션 저장·복원, 구버전 설정 마이그레이션, 실행 중 변경 금지 확인.
- 테스트 클라이언트는 받은 PCM을 재생하지 않고 수치로 검증함(동일 PC 피드백 방지).
- 별도 실물 서피스와 원격 PC 스피커에서의 실제 청취 및 출력 장치 변경은 미검증.
- H.264 지원 엔진/GUI 빌드 성공. 주 실행창 렌더링 확인.
- FreeRDP 루프백 클라이언트: AVC420, AVC444v2, GFX Planar 실제 복호화 성공.
- H.264 미지원 시 Planar, GFX 미지원 시 비트맵, 수동 비트맵 연결 성공.
- Microsoft RDP ActiveX 32비트: AVC420 및 AVC444v2 전송 후 시험 종료까지 연결 유지.
  16비트: 비트맵 전환 후 연결 유지. 이 검사는 RDCMan 원격 조작/LAN 성능 시험을 대신하지 않습니다.
- 합성 이동 영상: AVC420/AVC444/v2 각각 18 프레임 인코딩·복호화 및 RGB 오차 검사 통과.
  비정렬 해상도, 행 패딩, 부분 영역과 AVC444 보조 스트림 복호화 검사 통과.
- 신규 그래픽 모드에서도 올바른 암호만 화면 접근 허용, 암호 누락/오류·잘못된 계정 차단.
- 전송 FPS·페이로드·시간 계측값 유효성 및 로그 암호 미포함 확인.
- 구버전 설정 마이그레이션, 전송 모드 저장·복원, 실행 중 모드 변경 금지 검사.
- RDG 그룹/RDP 다중 가져오기 회귀 검사. 실제 설치된 RDCMan 설정 파서가 생성한 32비트 XML을 수용.
- DPAPI 설정 저장, 트레이 숨김·복원·종료 회귀 검사 통과. 실제 자동 시작 레지스트리는 변경하지 않음.
- 완성 배포 폴더를 Windows System32만 포함한 PATH에서 실행, AVC444v2 루프백 복호화 성공.
- 실물 Surface의 DXGI 캡처, 실제 LAN 스크롤 성능, 재부팅 자동 실행은 이번 환경에서 미검증.
- 개발 환경의 영상 접속 시험은 GDI 캡처를 사용함. 정지 화면의 낮은 FPS는 속도 측정 결과가 아님.
- CPU H.264 인코더이며 하드웨어 영상 인코딩 및 Windows WDDM 원격 세션 드라이버는 구현하지 않음.
\nGUI 검사\n'''+gui+'\n합성 영상 검사\n'+'\n'.join(a for a in codec.splitlines() if 'PASS' in a)+'\n접속 검사\n'+json.dumps(auth,ensure_ascii=False,indent=2)
(pkg/'오디오검증-개발PC.json').write_text(json.dumps(audio,indent=2),encoding='utf-8')
(pkg/'처리구조검증.txt').write_text(pipeline+'\n'+pacer,encoding='utf-8-sig')
(pkg/'성능비교-개발PC.json').write_text(json.dumps(bench,indent=2),encoding='utf-8')
(pkg/'검증결과.txt').write_text(validation,encoding='utf-8-sig')
(pkg/'그래픽검증-개발PC.json').write_text(json.dumps(graphics,indent=2),encoding='utf-8')
for p in [out/'SurfaceDesk-내서피스.rdg',out/'SurfaceDesk-192.168.0.110-3390.rdp']:shutil.copy2(p,pkg/p.name)
manifest={str(p.relative_to(pkg)):hashlib.sha256(p.read_bytes()).hexdigest() for p in pkg.rglob('*') if p.is_file() and p.name!='SHA256.json'}
(pkg/'SHA256.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf-8')
with zipfile.ZipFile(out/'SurfaceDesk-0.2.0-Windows-x64.zip','w',zipfile.ZIP_DEFLATED,6) as z:
 for p in pkg.rglob('*'):
  if p.is_file():z.write(p,str(p.relative_to(pkg.parent)))
with zipfile.ZipFile(out/'SurfaceDesk-0.2.0-Source.zip','w',zipfile.ZIP_DEFLATED,6) as z:
 for p in (w/'FreeRDP').rglob('*'):
  rel=p.relative_to(w/'FreeRDP')
  if '.git' not in rel.parts and p.is_file():z.write(p,'SurfaceDesk-Source/FreeRDP/'+rel.as_posix())
 names=['SurfaceDesk.cs','SurfaceDesk.Lifecycle.cs','DesktopOptions.cs','RdgManager.cs','PerformanceMetrics.cs','GraphicsMode.cs','TestDesktopOptions.cs','TestLifecycle017.cs','TestPerformanceMetrics.cs','TestRdg.cs','TestRdpImport.cs','TestGraphics018.cs','TestRdcSchema018.cs','probe.c','MicrosoftProbe.cs','Build.ps1','test_surface_diff.c','test_dxgi017.c','test_h264018.c','integration018.py','test_graphics018.py','build_gui018.py','test_gui018.py','build_tests018.py','deps018.py','deps018-manifest.json','SurfaceDesk.ico','SurfaceDesk-icon.svg','make_icon017.py','AudioMode.cs','test_wasapi019.c','test_audio019.py','integration019.py','test_graphics019.py','build_gui019.py','test_gui019.py','regression019.py','test_h264020.c','test_pipeline020.c','test_pacer020.c','benchmark020.c','bench020.py','build020.py','build_gui020.py','test_gui020.py','test_audio020.py','test_graphics020.py','integration020.py','regression020.py','validate020.py']
 for name in names:z.write(w/name,'SurfaceDesk-Source/'+name)
 for p in lic.iterdir():z.write(p,'SurfaceDesk-Source/licenses/'+p.name)
 z.writestr('SurfaceDesk-Source/FreeRDP/.source_version','dae0390a42ab271fa07a35979dfcd5671f83a949\n')
 z.writestr('SurfaceDesk-Source/FreeRDP/.source_tag','3.32.3-dev0\n')
 z.writestr('SurfaceDesk-Source/BUILD-INFO.txt','''Windows x64: LLVM-MinGW 20261020 UCRT x64, CMake 4.4.4, Ninja 1.13.2, .NET Framework 4.x compiler.
DependencyRoot: MSYS2 UCRT64 OpenSSL 3.6.5, zlib-ng-compat 2.3.3, OpenH264 2.7.0 headers and import libs.
Native runtime dependencies and licenses are listed in licenses/NOTICE.txt; OpenH264/runtime package hashes are pinned in deps018-manifest.json.
Build.ps1 -CompilerRoot <llvm-mingw-root> -DependencyRoot <ucrt64-root> -CMake <cmake.exe> -Ninja <ninja.exe>
Test targets surface-probe, surface-audio-probe, surface-codec-probe, surface-pipeline-probe, surface-pacer-probe and surface-benchmark are loopback/synthetic diagnostics and excluded from the runtime ZIP.
WASAPI system playback is enabled by SURFACE_DESK_AUDIO=1 (the GUI sets this). No microphone input is implemented.
CPU OpenH264 uses a low-complexity profile with up to 4 threads/slices. SURFACE_DESK_ENCODER_PROFILE=legacy enables the previous profile for diagnosis.
CPU OpenH264 is enabled. FFmpeg and Media Foundation encoders are not used. D3D11/DXGI are Windows system libraries.
Diagnostic Python scripts assume the development work/build-ready and work/runtime layout; adapt paths as necessary.
deps018.py uses Python zstandard (or work/tooling) and downloads verified dependency packages into work/deps.
TestRdcSchema018 requires an existing RDCMan executable supplied as argument; no actual server connection is made by that parser test.
''')
for name in ['SurfaceDesk-0.2.0-Windows-x64.zip','SurfaceDesk-0.2.0-Source.zip']:
 with zipfile.ZipFile(out/name) as z:assert z.testzip() is None
 print(name,(out/name).stat().st_size,hashlib.sha256((out/name).read_bytes()).hexdigest(),flush=True)
print('Packaged engine verified with clean PATH; archives verified')

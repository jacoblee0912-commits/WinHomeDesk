from pathlib import Path
import os,subprocess,shutil,zipfile,hashlib,json,ast,time,ctypes,re
w=Path(__file__).resolve().parent;out=w.parent/'outputs';pkg=w/'package/WinHomeDesk-0.2.7';engine=pkg/'engine';engine.mkdir(parents=True,exist_ok=True)
dlls=['libfreerdp-server3.dll','libfreerdp-shadow-subsystem3.dll','libfreerdp-shadow3.dll','libfreerdp3.dll','libssl-3-x64.dll','libcrypto-3-x64.dll','libwinpr-tools3.dll','libwinpr3.dll','zlib1.dll','libopenh264-8.dll','libgcc_s_seh-1.dll','libstdc++-6.dll','libwinpthread-1.dll']
for name in dlls+['surface-rdp-engine.exe']:shutil.copy2(w/'runtime'/name,engine/('WinHomeDesk-engine.exe' if name=='surface-rdp-engine.exe' else name))
(engine/'ossl-modules').mkdir(exist_ok=True);shutil.copy2(w/'deps/ucrt64/lib/ossl-modules/legacy.dll',engine/'ossl-modules/legacy.dll')
shutil.copy2(w/'WinHomeDesk.exe',pkg/'WinHomeDesk.exe');shutil.copy2(w/'SurfaceDesk.ico',pkg/'WinHomeDesk.ico')
for p in [out/'SurfaceDesk-내서피스.rdg',out/'SurfaceDesk-192.168.0.110-3390.rdp']:(pkg/p.name.replace('SurfaceDesk','WinHomeDesk')).write_bytes(p.read_bytes().replace(b'Surface Desk',b'WinHomeDesk'))
lic=pkg/'licenses';lic.mkdir(exist_ok=True)
for p in (w/'package/WinHomeDesk-0.2.6/licenses').iterdir():shutil.copy2(p,lic/p.name)
p=lic/'NOTICE.txt';p.write_text(p.read_text(encoding='utf-8-sig')+'\n0.2.7: disable unimplemented server UDP multitransport capability; match RDCMan 3.22 Shortpath configuration in tests. Retains application-account NLA/CredSSP/NTLMv2 plus legacy TLS. Uses Windows CNG MD4 only for protocol-required NT key derivation. RDCoreSdk/RDCMan proprietary binaries are NOT included.\n',encoding='utf-8')
results=json.loads((w/'compat027-results.json').read_text());assert len(results)==7 and all(x['result']=='PASS' for x in results)
before=(w/'rdcore027-before-server.log').read_text(errors='replace');after=(w/'rdcore027-server.log').read_text(errors='replace')
before_client=(w/'rdcore027-before-client.log').read_text();after_client=(w/'rdcore027-client.log').read_text()
assert 'CONNECTION_STATE_MULTITRANSPORT_BOOTSTRAPPING_RESPONSE' in before and ' Connected\n' not in before_client
assert 'CONNECTION_STATE_MULTITRANSPORT_BOOTSTRAPPING_RESPONSE' not in after and ' Connected\n' in after_client
assert 'CODEC name=avc420' in after and 'TRANSPORT tcp=1 udp_multitransport=0' in after
assert 'SURFACE_AUDIO_TEST_SOURCE:BOOL=OFF' in (w/'build-ready/CMakeCache.txt').read_text()
assert 'PASS' in (w/'rdg026-modern.log').read_text(encoding='utf-8-sig')
assert 'PASS' in (w/'rdg026-legacy.log').read_text(encoding='utf-8-sig')
# Test the packaged binaries, with both NLA and TLS advertised, on one server.
# Wrong credentials between two successful connections must never inherit auth.
secret='package-'+os.urandom(16).hex();salt=os.urandom(16)
bcrypt=ctypes.WinDLL('bcrypt');handle=ctypes.c_void_p();buf=ctypes.create_string_buffer(16)
assert bcrypt.BCryptOpenAlgorithmProvider(ctypes.byref(handle),'MD4',None,0)==0
raw=secret.encode('utf-16le');assert bcrypt.BCryptHash(handle,None,0,raw,len(raw),buf,16)==0
assert bcrypt.BCryptCloseAlgorithmProvider(handle,0)==0
env=os.environ.copy();env.update(PATH=os.environ['WINDIR']+'\\System32',WINHOME_DESK_NT_HASH=buf.raw.hex(),SURFACE_DESK_SALT=salt.hex(),SURFACE_DESK_HASH=hashlib.pbkdf2_hmac('sha1',secret.encode(),salt,200000,32).hex(),SURFACE_DESK_DATA=str(w/'package027-test-state'),OPENSSL_MODULES=str(engine/'ossl-modules'),SURFACE_DESK_CAPTURE='gdi',SURFACE_DESK_MAX_EDGE='1440',SURFACE_DESK_GPU='1',SURFACE_DESK_AUDIO='1',SURFACE_PROBE_GFX='420',SURFACE_PROBE_AUDIO='1',SURFACE_PROBE_AUDIO_STATS='1',SURFACE_PROBE_DURATION_MS='4000',WLOG_LEVEL='INFO')
with (w/'package027-test.log').open('w') as log:
 server=subprocess.Popen([str(engine/'WinHomeDesk-engine.exe'),'/bind-address:127.0.0.1','/port:13390','/sec:tls','+auth','+may-view','+may-interact','/max-connections:1','+gfx','+gfx-avc420','-gfx-avc444','+gfx-planar','-gfx-progressive','-rfx','-nsc','+bitmap-compat'],env=env,cwd=engine,stdout=log,stderr=subprocess.STDOUT)
 try:
  time.sleep(1);assert server.poll() is None
  for completed,correct in enumerate([True,False,True],1):
   p=subprocess.run([str(w/'runtime/surface-probe.exe'),secret if correct else secret+'-wrong'],env=env,cwd=w/'runtime',capture_output=True,timeout=20)
   text=p.stdout.decode(errors='replace')+p.stderr.decode(errors='replace')
   if correct:assert p.returncode==0 and re.search(r'gfx_decoded=[1-9]',text) and 'codec=11' in text,text[-1500:]
   else:assert p.returncode!=0 and not re.search(r'gfx_decoded=[1-9]',text),text[-1500:]
   # A max-connections=1 server releases its slot after channel teardown.
   # Wait for the explicit completed-close marker, not just client process exit.
   for _ in range(150):
    if (w/'package027-test.log').read_text(errors='replace').count('SESSION closed')>=completed:break
    time.sleep(.1)
   else:raise AssertionError('previous connection did not finish cleanup')
 finally:
  server.terminate();server.wait(timeout=5)
text=(w/'package027-test.log').read_text(errors='replace')
assert text.count('authentication: accepted')==2 and text.count('authentication: rejected')==1
assert 'backend=gpu' in text and 'AUDIO_CHANNEL state=ready' in text and 'rate=44100' in text
assert 'AUDIO_TEST' not in text and secret not in text and buf.raw.hex() not in text
guide=(w/'사용안내027.txt').read_text(encoding='utf-8')
(pkg/'사용안내.txt').write_text(guide,encoding='utf-8-sig');(out/'WinHomeDesk-사용안내.txt').write_text(guide,encoding='utf-8-sig')
validation='''WinHomeDesk 0.2.7 검증 결과
- RDCMan 3.22의 실제 RDCoreSdk 및 앱과 같은 ConnectionMode/UDP Shortpath/SmartSizing 설정으로 NLA 요청/인증/H.264 프레임 전송 및 12초 연결 유지 확인.
- 수정 전 동일 설정은 MULTITRANSPORT_BOOTSTRAPPING_RESPONSE에서 대기, 수정 후 같은 설정으로 연결 성공.
- 서버가 지원하지 않는 UDP multitransport를 광고하지 않도록 수정. TCP 및 NLA/TLS 유지.
- RDCore의 TLS 전용 서버 연결, 잘못된 암호/계정 거절, 한글 포함 암호 확인.
- Microsoft MSTSC ActiveX의 NLA 인증과 화면 전송 확인.
- 같은 ActiveX 엔진의 기존 TLS 전용 서버 연결도 회귀 확인.
- NLA/TLS 동시 활성 서버에 기존 FreeRDP TLS 클라이언트 접속 및 프레임 디코딩 확인.
- 2.93/3.22 실제 RDG SecuritySettings 파서 오류 없이 읽기, 새 엔진 CredSSP 기본 켜짐 확인.
- CNG NT 해시 알려진 벡터 확인, GUI 설정/암호화/트레이/수명 회귀 통과.
- 최종 배포 DLL을 제한된 PATH에서 실행: TLS 정상→잘못된 암호→정상 재접속.
  성공 연결 두 번만 인증·영상 전송 허용. GPU AVC420 및 44.1kHz 오디오 채널 준비 확인.
- 오디오 형식 배열을 연결별로 소유하도록 수정하여 공유/static 배열의 잘못된 해제를 방지.
  세 연결 모두 SESSION closed로 자원 정리가 끝나는지 확인한 뒤 다음 연결을 시험했습니다.
- 실제 서피스 LAN에서 RDCMan 전체 UI의 재접속, 장시간 안정성은 이번 시험 범위 밖입니다.
- 영상/소리 성능과 한국어 IME 조합은 이번 연결 호환성 변경에서 다시 측정하지 않았습니다.
- Microsoft의 RDCMan/RDCoreSdk DLL은 시험에만 사용했으며 배포본·소스 ZIP에 포함하지 않습니다.
'''
(pkg/'검증결과.txt').write_text(validation+'\n'+json.dumps(results,ensure_ascii=False,indent=2),encoding='utf-8-sig')
shutil.copy2(w/'compat027-results.json',pkg/'연결호환성검증.json')
(pkg/'재현비교.json').write_text(json.dumps({'before':'authentication accepted, waits at MULTITRANSPORT_BOOTSTRAPPING_RESPONSE, no Connected event','after':'same RDCore app options; NLA connected and AVC420 frames transmitted','client_options':{'EnableUDPShortpath':True,'ConnectionMode':0,'EnableSmartSizing':True,'SingleMonInWindowedMode':True},'server_fix':'SupportMultitransport=false, MultitransportFlags=0'},indent=2))
manifest={str(p.relative_to(pkg)):hashlib.sha256(p.read_bytes()).hexdigest() for p in pkg.rglob('*') if p.is_file() and p.name!='SHA256.json'}
(pkg/'SHA256.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf-8')
runtime_zip=out/'WinHomeDesk-0.2.7-Windows-x64.zip';source_zip=out/'WinHomeDesk-0.2.7-Source.zip'
with zipfile.ZipFile(runtime_zip,'w',zipfile.ZIP_DEFLATED,6) as z:
 for p in pkg.rglob('*'):
  if p.is_file():z.write(p,p.relative_to(pkg.parent))
# Reuse the explicit source manifests, never bundle development test binaries.
names=[]
for script in ['package020.py','package025.py']:
 for node in ast.walk(ast.parse((w/script).read_text(encoding='utf-8-sig'))):
  if isinstance(node,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='names' for t in node.targets) and isinstance(node.value,ast.List):names+=ast.literal_eval(node.value)
  if isinstance(node,ast.AugAssign) and isinstance(node.target,ast.Name) and node.target.id=='names' and isinstance(node.value,ast.List):names+=ast.literal_eval(node.value)
names+=['AppCredentials.cs','TestCredentials026.cs','build_gui027.py','test_gui026.py','test_compat027.py','rdcoreprobe027.cs','rdcoreprobe027.ps1','build_rdgtest026.py','package027.py','사용안내027.txt']
with zipfile.ZipFile(source_zip,'w',zipfile.ZIP_DEFLATED,6) as z:
 for p in (w/'FreeRDP').rglob('*'):
  rel=p.relative_to(w/'FreeRDP')
  if '.git' not in rel.parts and p.is_file():z.write(p,'WinHomeDesk-Source/FreeRDP/'+rel.as_posix())
 for name in sorted(set(names)):z.write(w/name,'WinHomeDesk-Source/'+name)
 for p in lic.iterdir():z.write(p,'WinHomeDesk-Source/licenses/'+p.name)
 z.writestr('WinHomeDesk-Source/FreeRDP/.source_version','dae0390a42ab271fa07a35979dfcd5671f83a949\n')
 z.writestr('WinHomeDesk-Source/FreeRDP/.source_tag','3.32.3-dev0\n')
 z.writestr('WinHomeDesk-Source/BUILD-INFO.txt','''Windows x64. Build.ps1 is the production build entry point; includes AppCredentials.cs.
LLVM-MinGW 20261020 UCRT x64, CMake 4.4.4, Ninja 1.13.2, .NET Framework 4.x csc.
Dependencies: MSYS2 UCRT64 OpenSSL 3.6.5, zlib-ng-compat 2.3.3, OpenH264 2.7.0.
Production SURFACE_AUDIO_TEST_SOURCE=OFF. No Windows system or RDCMan DLLs redistributed.
Rename shadow CLI to engine/WinHomeDesk-engine.exe; package runtime DLLs and legacy OpenSSL provider.
NLA uses per-process WINHOME_DESK_NT_HASH plus existing PBKDF2 app credentials. Do not store these in files.
RDCore test harness requires your separately licensed installed RDCMan 3.22 SDK assemblies under work/rdc322-bundle,
PowerShell/.NET 10 and WindowsDesktop runtime (adjust installed version in rdcoreprobe027.ps1).
The harness accepts the loopback test certificate only and must not be used for remote targets.
Historical test scripts retain their original names; use build_gui027.py for the current GUI.
''')
for p in [runtime_zip,source_zip]:
 with zipfile.ZipFile(p) as z:
  assert z.testzip() is None
  assert not any('rdcoresdk.dll' in n.lower() or 'rdcman.dll' in n.lower() for n in z.namelist())
 print(p.name,p.stat().st_size,hashlib.sha256(p.read_bytes()).hexdigest(),flush=True)
print('PASS packaged authentication isolation, GPU/audio, archive integrity',flush=True)

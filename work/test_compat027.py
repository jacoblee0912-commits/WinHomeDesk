from pathlib import Path
import os,subprocess,hashlib,time,ctypes,json,re,sys
w=Path(__file__).resolve().parent
bcrypt=ctypes.WinDLL('bcrypt')
def nthash(secret):
 handle=ctypes.c_void_p();buf=ctypes.create_string_buffer(16)
 assert bcrypt.BCryptOpenAlgorithmProvider(ctypes.byref(handle),'MD4',None,0)==0
 raw=secret.encode('utf-16le')
 assert bcrypt.BCryptHash(handle,None,0,raw,len(raw),buf,16)==0
 assert bcrypt.BCryptCloseAlgorithmProvider(handle,0)==0
 return buf.raw.hex()
assert nthash('password')=='8846f7eaee8fb117ad06bdd830b7586c'
results=[]
cases=['rdcore-nla','rdcore-tls','rdcore-wrong-password','rdcore-wrong-user','mstsc-nla','freerdp-tls','rdcore-unicode']
if len(sys.argv)>1:cases=sys.argv[1:]
for case in cases:
 secret='probe-'+os.urandom(16).hex()+('한글é' if case=='rdcore-unicode' else '');salt=os.urandom(16)
 env=os.environ.copy()
 env.update(WINHOME_TEST_PASSWORD=secret,WINHOME_DESK_NT_HASH=nthash(secret),SURFACE_DESK_SALT=salt.hex(),SURFACE_DESK_HASH=hashlib.pbkdf2_hmac('sha1',secret.encode(),salt,200000,32).hex(),SURFACE_DESK_DATA=str(w/'test-state'),OPENSSL_MODULES=str(w/'deps/ucrt64/lib/ossl-modules'),SURFACE_DESK_CAPTURE='gdi',SURFACE_DESK_FPS='20',SURFACE_DESK_MAX_EDGE='1440',SURFACE_DESK_GPU='1',SURFACE_DESK_AUDIO='0',WLOG_LEVEL='INFO',SURFACE_PROBE_DURATION_MS='5000',SURFACE_PROBE_GFX='420')
 env.pop('WINHOME_PROBE_TLS',None);env.pop('WINHOME_PROBE_USER',None)
 if case=='rdcore-tls':env['WINHOME_PROBE_TLS']='1';env.pop('WINHOME_DESK_NT_HASH')
 if case=='rdcore-wrong-password':env['WINHOME_TEST_PASSWORD']+='-incorrect'
 if case=='rdcore-wrong-user':env['WINHOME_PROBE_USER']='not-surface'
 path=w/f'compat027-{case}-server.log';report=w/f'compat027-{case}-client.log'
 report.write_text('')
 with path.open('w') as log:
  server=subprocess.Popen([str(w/'runtime/surface-rdp-engine.exe'),'/bind-address:127.0.0.1','/port:13390','/sec:tls','+auth','+may-view','+may-interact','/max-connections:1','-rfx','-nsc','+bitmap-compat','-gfx-progressive','+gfx','+gfx-planar','+gfx-avc420','-gfx-avc444'],env=env,stdout=log,stderr=subprocess.STDOUT,cwd=w/'runtime')
  try:
   time.sleep(1);assert server.poll() is None
   if case.startswith('rdcore'):
    cmd=['pwsh','-STA','-NoProfile','-File',str(w/'rdcoreprobe027.ps1'),'-Report',str(report)]
   elif case.startswith('mstsc'):cmd=[str(w/'MicrosoftProbe.exe'),secret,str(report)]
   else:cmd=[str(w/'runtime/surface-probe.exe'),secret]
   p=subprocess.run(cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=35,cwd=w/'runtime')
   output=p.stdout.decode('utf-8',errors='replace');(w/f'compat027-{case}-process.log').write_text(output,encoding='utf-8')
   assert p.returncode==0,output[-2500:]
  finally:
   server.terminate();server.wait(timeout=5)
 text=path.read_text(errors='replace');client=report.read_text(encoding='utf-8-sig')
 assert 'TRANSPORT tcp=1 udp_multitransport=0' in text
 assert secret not in text+client and nthash(secret).lower() not in text.lower()
 rejected='wrong' in case
 if rejected:
  assert 'authentication: accepted' not in text and 'CODEC name=' not in text
  assert any(x in text for x in ('SEC_E_LOGON_DENIED','SEC_E_MESSAGE_ALTERED','STATUS_LOGON_FAILURE','SEC_E_NO_CREDENTIALS')),text[-2000:]
  assert 'CredentialsRequested reason=InvalidCredentials' in client,client
  assert 'Connected\n' not in client
 else:
  assert 'authentication: accepted' in text and 'CODEC name=avc420' in text,text[-2500:]
  assert re.search(r'PERF_SEND state=active .*frames=[1-9]',text),text[-2000:]
  protocol=1 if case in ('rdcore-tls','freerdp-tls') else 2
  assert f'SESSION security_protocol={protocol} ' in text,text[-2000:]
  if case.startswith('rdcore'):
   assert 'CredentialsRequested' in client and ' Connected\n' in client
   assert 'Disconnected=' not in client.split('Timeout test end')[0],client
  elif case.startswith('mstsc'):assert 'connected=True' in client and 'OnDisconnected=' not in client,client
  else:assert re.search(r'gfx_decoded=[1-9]',output),output[-1000:]
 results.append(dict(case=case,result='PASS',security='rejected' if rejected else ('TLS' if protocol==1 else 'NLA'),frames_transmitted=not rejected))
 print(case,'PASS',flush=True)
(w/('compat027-results'+('-'+'-'.join(sys.argv[1:]) if len(sys.argv)>1 else '')+'.json')).write_text(json.dumps(results,indent=2))

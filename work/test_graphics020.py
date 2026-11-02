from pathlib import Path
import os,subprocess,shutil,hashlib,socket,time,json,re,math
r=Path.cwd();w=r/'work';runtime=w/'runtime'
for p in (w/'build-ready').rglob('*.dll'):shutil.copy2(p,runtime/p.name)
for p in (w/'deps/ucrt64/bin').glob('*.dll'):shutil.copy2(p,runtime/p.name)
shutil.copy2(w/'build-ready/server/shadow/cli/freerdp-shadow-cli.exe',runtime/'surface-rdp-engine.exe')
shutil.copy2(w/'build-ready/surface-probe.exe',runtime/'surface-probe.exe')
results=[]
cases=[('avc420','420',0,'avc420',11),('avc444','444',1,'avc444v2',15),
 ('avc444-v1','444',1,'avc444',14),('fallback420','420',1,'avc420',11),
 ('planar','planar',2,'gfx-planar',10),('fallback-planar','planar',0,'gfx-planar',10),
 ('fallback-bitmap',None,0,'bitmap',0),('bitmap',None,3,'bitmap',0),
 ('microsoft',None,0,'avc420',None),('microsoft444',None,1,'avc444v2',None),
 ('microsoft16',None,0,'bitmap',None)]
if len(os.sys.argv)>1:cases=[a for a in cases if a[0]==os.sys.argv[1]]
for name,gfx,mode,expected,codec in cases:
 secret='probe-'+os.urandom(16).hex();salt=os.urandom(16)
 env=os.environ.copy();env.pop('SURFACE_PROBE_GFX',None)
 env.update(SURFACE_DESK_SALT=salt.hex(),SURFACE_DESK_HASH=hashlib.pbkdf2_hmac('sha1',secret.encode(),salt,200000,32).hex(),SURFACE_DESK_DATA=str(w/'test-state'),OPENSSL_MODULES=str(w/'deps/ucrt64/lib/ossl-modules'),SURFACE_DESK_CAPTURE='gdi',SURFACE_DESK_FPS='30',SURFACE_DESK_MAX_EDGE='1440',SURFACE_PROBE_DURATION_MS='6500',SURFACE_PROBE_REFRESH_MS='200')
 if gfx:env['SURFACE_PROBE_GFX']=gfx
 env.pop('SURFACE_PROBE_CAPS10',None)
 if name=='avc444-v1':env['SURFACE_PROBE_CAPS10']='1'
 env.pop('SURFACE_MS_COLOR',None)
 if name=='microsoft16':env['SURFACE_MS_COLOR']='16'
 flags=['-rfx','-nsc','+bitmap-compat','-gfx-progressive']
 if mode==3:flags+=['-gfx','-gfx-avc420','-gfx-avc444']
 else:flags+=['+gfx','+gfx-planar','-gfx-avc420' if mode==2 else '+gfx-avc420','+gfx-avc444' if mode==1 else '-gfx-avc444']
 path=w/f'gfx020-{name}.log'
 with path.open('w') as log:
  server=subprocess.Popen([str(runtime/'surface-rdp-engine.exe'),'/bind-address:127.0.0.1','/port:13390','/sec:tls','+auth','+may-view','+may-interact','/max-connections:1',*flags],env=env,stdout=log,stderr=subprocess.STDOUT,cwd=runtime)
  try:
   for _ in range(100):
    assert server.poll() is None,'server exited: '+path.read_text(errors='replace')[-1500:]
    try:
     with socket.create_connection(('127.0.0.1',13390),timeout=.2):break
    except OSError:time.sleep(.1)
   else:raise RuntimeError('listener timeout')
   report=w/f'{name}020.log'
   if name.startswith('microsoft'):report.write_text('')
   command=[str(w/'MicrosoftProbe.exe'),secret,str(report)] if name.startswith('microsoft') else [str(runtime/'surface-probe.exe'),secret]
   probe=subprocess.run(command,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=25,cwd=runtime)
   output=probe.stdout.decode(errors='replace');(w/f'gfx020-{name}-client.log').write_text(output,encoding='utf-8')
   assert probe.returncode==0,output[-2500:]+'\nSERVER:\n'+path.read_text(errors='replace')[-2000:]
   time.sleep(.5)
  finally:
   if server.poll() is None:server.terminate();server.wait(timeout=5)
 text=path.read_text(errors='replace');assert secret not in text and salt.hex() not in text
 assert 'authentication: accepted' in text
 if name.startswith('microsoft'):
  ms=report.read_text(encoding='utf-8-sig');assert 'connected=True' in ms and 'OnDisconnected=' not in ms,ms
 used=re.findall(r'SurfaceDesk CODEC name=(\S+)',text)
 assert used and (expected is None or used[-1]==expected),used
 if gfx:
  m=re.search(r'gfx_decoded=(\d+) codec=(\d+)',output);assert m and int(m[1])>=1 and int(m[2])==codec,output[-1500:]
 rows=[]
 for line in text.splitlines():
  if not line.startswith('SurfaceDesk PERF_SEND '):continue
  fields=dict(token.split('=',1) for token in line.split()[2:]);state=fields.pop('state');fields={k:float(v) for k,v in fields.items()}
  assert all(math.isfinite(v) and v>=0 for v in fields.values())
  assert fields['other']==0 and fields['area_pct']<=100.01
  if state=='active':assert fields['failed']==0
  # window_s is printed to three decimals; allow that rounding on short final windows.
  assert abs(fields['frames']/fields['window_s']-fields['fps'])<.01+fields['fps']*.0006/fields['window_s']
  assert abs(fields['payload_bytes']*8/fields['window_s']/1e6-fields['payload_mbps'])<.002+fields['payload_mbps']*.0006/fields['window_s']
  rows.append(dict(state=state,**fields))
 assert any(a['frames']>0 and a['payload_bytes']>0 for a in rows)
 results.append(dict(case=name,codecs=used,probe=[a for a in output.splitlines() if a.startswith('PROBE')],metrics=rows))
 print(name,used,'PASS',flush=True)
(w/('graphics020-results'+('-'+os.sys.argv[1] if len(os.sys.argv)>1 else '')+'.json')).write_text(json.dumps(results,indent=2),encoding='utf-8')

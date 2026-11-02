from pathlib import Path
import subprocess,os,hashlib,socket,time,json
w=Path.cwd()/'work';runtime=w/'runtime';secret='test-'+os.urandom(16).hex();salt=os.urandom(16)
env=os.environ.copy();env.update(SURFACE_DESK_SALT=salt.hex(),SURFACE_DESK_HASH=hashlib.pbkdf2_hmac('sha1',secret.encode(),salt,200000,32).hex(),SURFACE_DESK_DATA=str(w/'lifecycle021-state'),OPENSSL_MODULES=str(runtime/'ossl-modules'),SURFACE_DESK_CAPTURE='gdi',SURFACE_DESK_MAX_EDGE='1440',SURFACE_DESK_GPU='1',SURFACE_DESK_FPS='45',SURFACE_PROBE_GFX='420',SURFACE_PROBE_REFRESH_MS='100')
results=[]
with (w/'lifecycle021-server.log').open('w') as log:
 server=subprocess.Popen([str(runtime/'surface-rdp-engine.exe'),'/bind-address:127.0.0.1','/port:13390','/sec:tls','+auth','+may-view','+may-interact','/max-connections:1','+gfx','+gfx-avc420','-gfx-avc444','+gfx-planar','-gfx-progressive','-rfx','-nsc','+bitmap-compat'],env=env,cwd=runtime,stdout=log,stderr=subprocess.STDOUT)
 try:
  for _ in range(100):
   assert server.poll() is None
   try:
    with socket.create_connection(('127.0.0.1',13390),timeout=.2):break
   except OSError:time.sleep(.1)
  else:raise RuntimeError('listener timeout')
  for name,duration,suppress in [('early-disconnect1','200',False),('early-disconnect2','200',False),('reconnect','2500',False),('suppress-resume','3500',True),('reconnect-after-suppress','2500',False)]:
   e=env.copy();e['SURFACE_PROBE_DURATION_MS']=duration
   if suppress:e['SURFACE_PROBE_SUPPRESS']='1'
   p=subprocess.run([str(runtime/'surface-probe.exe'),secret],env=e,cwd=runtime,capture_output=True,timeout=12)
   text=p.stdout.decode(errors='replace')+p.stderr.decode(errors='replace');(w/('lifecycle021-'+name+'.log')).write_text(text,encoding='utf-8')
   if not name.startswith('early'):assert p.returncode==0,text[-1500:]
   assert server.poll() is None
   results.append(dict(case=name,exit=p.returncode,probe=[a for a in text.splitlines() if a.startswith('PROBE')]))
   time.sleep(1)
 finally:
  if server.poll() is None:server.terminate();server.wait(timeout=5)
text=(w/'lifecycle021-server.log').read_text(errors='replace');assert 'backend=gpu' in text
(w/'lifecycle021-results.json').write_text(json.dumps(results,indent=2))
print('Early disconnect, reconnect and suppress/resume PASS')

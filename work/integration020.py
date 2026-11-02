from pathlib import Path
import shutil,subprocess,os,hashlib,socket,time,json
r=Path.cwd();stage=r/'work/runtime';stage.mkdir(exist_ok=True)
for p in (r/'work/build-ready').rglob('*.dll'):shutil.copy2(p,stage/p.name)
for p in (r/'work/deps/ucrt64/bin').glob('*.dll'):shutil.copy2(p,stage/p.name)
shutil.copy2(r/'work/build-ready/server/shadow/cli/freerdp-shadow-cli.exe',stage/'surface-rdp-engine.exe')
shutil.copy2(r/'work/build-ready/surface-probe.exe',stage/'surface-probe.exe')
password='test-only-'+os.urandom(16).hex();salt=os.urandom(16)
env=os.environ.copy();env['SURFACE_PROBE_GFX']='420';env['SURFACE_DESK_SALT']=salt.hex();env['SURFACE_DESK_HASH']=hashlib.pbkdf2_hmac('sha1',password.encode(),salt,200000,32).hex();env['SURFACE_DESK_DATA']=str(r/'work/test-state');env['WLOG_LEVEL']='WARN';env['OPENSSL_MODULES']=str(r/'work/deps/ucrt64/lib/ossl-modules')
logs=open('work/server-auth020.log','w')
server=subprocess.Popen([str(stage/'surface-rdp-engine.exe'),'/bind-address:127.0.0.1','/port:13390','/sec:tls','+auth','+may-view','+may-interact','/max-connections:1','+gfx','+gfx-avc420','-gfx-avc444','+gfx-planar','-gfx-progressive','-rfx','-nsc','+bitmap-compat'],env=env,cwd=stage,stdout=logs,stderr=subprocess.STDOUT)
results={}
try:
 for n in range(100):
  if server.poll() is not None:raise RuntimeError('server exited '+str(server.returncode))
  try:
   with socket.create_connection(('127.0.0.1',13390),timeout=.2):break
  except OSError:time.sleep(.1)
 else:raise RuntimeError('listener timeout')
 for name,pw in [('wrong_password','incorrect-password'),('missing_password',''),('wrong_username',password),('case_username',password),('correct_password',password),('microsoft_client',password)]:
  command=[str(stage/'surface-probe.exe'),pw] if name!='microsoft_client' else [str(r/'work/MicrosoftProbe.exe'),pw,str(r/'work/microsoft-auth020.log')]
  if name=='wrong_username':command.append('incorrect-user')
  if name=='case_username':command.append('Surface')
  res=subprocess.run(command,env=env,cwd=stage,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=20)
  text=res.stdout.decode(errors='replace');Path('work/'+name+'.log').write_text(text,encoding='utf-8');results[name]={'exit':res.returncode,'probe':[line for line in text.splitlines() if 'PROBE' in line]}
  time.sleep(.5)
finally:
 if server.poll() is None:server.terminate();server.wait(timeout=5)
 logs.close()
Path('work/auth020-results.json').write_text(json.dumps(results,indent=2),encoding='utf-8');print(json.dumps(results));print(Path('work/server-auth020.log').read_text(errors='replace')[-4000:])
for name in ['wrong_password','missing_password','wrong_username']:assert results[name]['exit']==1,name
for name in ['correct_password','case_username','microsoft_client']:assert results[name]['exit']==0,name
server_log=Path('work/server-auth020.log').read_text(errors='replace')
for reason in ['password mismatch or derivation failed','password not supplied by client','username must be surface','authentication: accepted']:assert reason in server_log,reason
assert password not in server_log

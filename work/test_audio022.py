from pathlib import Path
import os,subprocess,shutil,hashlib,socket,time,json,re
w=Path.cwd()/'work';runtime=w/'runtime'
for p in (w/'build-ready').rglob('*.dll'):shutil.copy2(p,runtime/p.name)
for p in (w/'deps/ucrt64/bin').glob('*.dll'):shutil.copy2(p,runtime/p.name)
shutil.copy2(w/'build-ready/server/shadow/cli/freerdp-shadow-cli.exe',runtime/'surface-rdp-engine.exe')
shutil.copy2(w/'build-ready/surface-probe.exe',runtime/'surface-probe.exe')
results=[]
for name,serverAudio,clientAudio,auth in [('sound',True,True,True),('sound-delay',True,True,True),('sound-legacy',True,True,True),('off',False,True,True),('client-disabled',True,False,True),('wrong-password',True,True,False)]:
 secret='audio-test-'+os.urandom(16).hex();salt=os.urandom(16);env=os.environ.copy()
 env.update(SURFACE_DESK_SALT=salt.hex(),SURFACE_DESK_HASH=hashlib.pbkdf2_hmac('sha1',secret.encode(),salt,200000,32).hex(),SURFACE_DESK_DATA=str(w/'test-state'),OPENSSL_MODULES=str(w/'deps/ucrt64/lib/ossl-modules'),SURFACE_DESK_CAPTURE='gdi',SURFACE_DESK_FPS='30',SURFACE_DESK_MAX_EDGE='1440',SURFACE_PROBE_DURATION_MS='7000',SURFACE_PROBE_REFRESH_MS='500',SURFACE_PROBE_GFX='420',SURFACE_PROBE_AUDIO_STATS='1',SURFACE_DESK_AUDIO='1' if serverAudio else '0')
 env.pop('SURFACE_PROBE_AUDIO',None)
 if clientAudio:env['SURFACE_PROBE_AUDIO']='1'
 env.pop('SURFACE_PROBE_AUDIO_HOLD_MS',None);env.pop('SURFACE_PROBE_AUDIO_LEGACY',None)
 if name=='sound-delay':env['SURFACE_PROBE_AUDIO_HOLD_MS']='350'
 if name=='sound-legacy':env['SURFACE_PROBE_AUDIO_LEGACY']='1'
 path=w/f'audio022-{name}.log'
 with path.open('w') as log:
  server=subprocess.Popen([str(runtime/'surface-rdp-engine.exe'),'/bind-address:127.0.0.1','/port:13390','/sec:tls','+auth','+may-view','+may-interact','/max-connections:1','+gfx','+gfx-avc420','-gfx-avc444','+gfx-planar','-gfx-progressive','-rfx','-nsc','+bitmap-compat'],env=env,cwd=runtime,stdout=log,stderr=subprocess.STDOUT)
  try:
   for _ in range(100):
    assert server.poll() is None,'server exited'
    try:
     with socket.create_connection(('127.0.0.1',13390),timeout=.2):break
    except OSError:time.sleep(.1)
   else:raise RuntimeError('listener timeout')
   with (w/f'audio022-{name}-client.log').open('w') as clientLog:
    probe=subprocess.Popen([str(runtime/'surface-probe.exe'),secret if auth else 'incorrect-password'],env=env,cwd=runtime,stdout=clientLog,stderr=subprocess.STDOUT)
    try:
     if name.startswith('sound'):
      time.sleep(2)
      tone=subprocess.run([str(w/'build-ready/surface-audio-probe.exe')],capture_output=True,timeout=10)
      assert tone.returncode==0,tone.stdout.decode(errors='replace')
      (w/'wasapi022-test.txt').write_bytes(tone.stdout)
     code=probe.wait(timeout=20)
    finally:
     if probe.poll() is None:probe.terminate();probe.wait(timeout=5)
   assert server.poll() is None,'audio caused server exit'
   time.sleep(.3)
  finally:
   if server.poll() is None:server.terminate();server.wait(timeout=5)
 text=path.read_text(errors='replace');output=(w/f'audio022-{name}-client.log').read_text(errors='replace')
 rows=re.findall(r'PROBE_AUDIO bytes=(\d+) nonzero=(\d+) peak=(\d+)',output)
 total=sum(int(row[0]) for row in rows);nonzero=sum(int(row[1]) for row in rows)
 if auth:assert code==0,output[-1800:]+'\nSERVER:\n'+text[-1800:]
 else:assert code!=0 and 'authentication: rejected' in text
 if name.startswith('sound'):
  assert total>192000 and nonzero>1000,(rows,text[-4000:],output[-2000:])
  assert 'PROBE_AUDIO_FORMAT tag=1 rate=48000 channels=2 bits=16' in output
  assert 'AUDIO_CHANNEL state=ready' in text and 'AUDIO_SENT frames=' in text
  assert 'CODEC name=avc420' in text
  packets=re.search(r'PROBE_AUDIO_PACKETS count=(\d+) min_bytes=(\d+) max_bytes=(\d+)',output)
  assert packets and int(packets[1])>100 and packets[2]=='1920' and packets[3]=='1920',output[-2000:]
  metrics=[dict(token.split('=',1) for token in line.split()[2:]) for line in text.splitlines() if line.startswith('SurfaceDesk AUDIO_LATENCY ')]
  assert any(int(a['confirmed'])>0 for a in metrics),text[-2000:]
  if name=='sound-delay':assert any(float(a['client_hold_ms'])>=300 for a in metrics),metrics
  else:assert all(float(a['client_hold_ms'])<200 for a in metrics),metrics
  if name=='sound-legacy':assert 'PROBE_AUDIO_WAVE2' not in output
  else:
   wave=re.findall(r'PROBE_AUDIO_WAVE2 bytes=(\d+) capture_age_ms=(-?\d+) capture_timestamp=(\d+)',output)
   assert wave and all(a[0]=='1920' and abs(int(a[1]))<500 and int(a[2])>65535 for a in wave),wave
  assert 'AUDIO_BUFFER requested_ms=40 actual_ms=' in text
 else:assert total==0,(name,total)
 if not auth:assert 'AUDIO_CHANNEL state=ready' not in text
 if not serverAudio:assert 'AUDIO state=capturing' not in text
 assert secret not in text and salt.hex() not in text
 results.append(dict(case=name,exit=code,bytes=total,nonzero=nonzero,probe=[a for a in output.splitlines() if a.startswith('PROBE')]))
 print(name,'PASS',total,'bytes',flush=True)
(w/'audio022-results.json').write_text(json.dumps(results,indent=2),encoding='utf-8')

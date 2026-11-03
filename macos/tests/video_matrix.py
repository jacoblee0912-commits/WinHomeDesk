#!/usr/bin/env python3
"""Real authenticated RDP negotiation + encode/decode of deterministic test pixels.
Synthetic capture is loopback-only and never posts OS input or selects an IME.
"""
import ctypes, hashlib, json, os, pathlib, re, secrets, socket, subprocess, tempfile, time
ROOT=pathlib.Path(__file__).resolve().parents[2]
MAC=pathlib.Path(os.environ.get('WINHOMEDESK_TEST_APP',ROOT/'output/macos/WinHomeDesk.app'))/'Contents/MacOS'
common=ctypes.CDLL('/usr/lib/system/libcommonCrypto.dylib')
common.CC_MD4.argtypes=[ctypes.c_void_p,ctypes.c_uint32,ctypes.c_void_p]
results=[]
cases=[('avc420_gpu',0,0,1,20,'avc420','videotoolbox'),
       ('avc420_cpu',0,0,0,30,'avc420','openh264'),
       ('avc444_cpu',1,1,0,45,'avc444v2','openh264'),
       ('avc444_gpu_preference',1,1,1,20,'avc444v2','openh264'),
       ('planar',2,2,1,30,'gfx-planar',None),
       ('bitmap',3,3,1,45,'bitmap',None),
       ('avc444_to_420',1,0,0,30,'avc420','openh264'),
       ('gfx_to_bitmap',0,3,1,20,'bitmap',None)]
for name,server_mode,client_mode,gpu,fps,codec,encoder in cases:
    if os.environ.get("VIDEO_CASE") and name!=os.environ["VIDEO_CASE"]:continue
    with tempfile.TemporaryDirectory(prefix='whd-video-') as tmp:
        secret=secrets.token_urlsafe(18);salt=secrets.token_bytes(16)
        nt=(ctypes.c_ubyte*16)(); raw=secret.encode('utf-16le');common.CC_MD4(raw,len(raw),nt)
        with socket.socket() as sock:sock.bind(('127.0.0.1',0));port=sock.getsockname()[1]
        env={**os.environ,'XDG_CONFIG_HOME':tmp,'SURFACE_DESK_DATA':tmp,
             'SURFACE_DESK_SALT':salt.hex(),'SURFACE_DESK_HASH':hashlib.pbkdf2_hmac('sha1',secret.encode(),salt,200000,32).hex(),
             'WINHOME_DESK_NT_HASH':bytes(nt).hex(),'WINHOMEDESK_TEST_PATTERN':'1',
             'SURFACE_DESK_GPU':str(gpu),'SURFACE_DESK_FPS':str(fps)}
        args=['-rfx','-nsc','+bitmap-compat','-gfx-progressive']
        args+=['-gfx','-gfx-avc420','-gfx-avc444'] if server_mode==3 else [
            '+gfx','+gfx-planar','-gfx-avc420' if server_mode==2 else '+gfx-avc420',
            '+gfx-avc444' if server_mode==1 else '-gfx-avc444']
        log=pathlib.Path(tmp)/'server.log'
        with log.open('w') as out:
            server=subprocess.Popen([str(MAC/'WinHomeDeskServer'),f'/port:{port}','/bind-address:127.0.0.1',*args],env=env,stdout=out,stderr=out)
            try:
                deadline=time.monotonic()+15
                while 'test-pattern-no-input' not in log.read_text() and time.monotonic()<deadline and server.poll() is None:time.sleep(.1)
                p=subprocess.run([str(MAC/'WinHomeDesk'),'--probe','127.0.0.1',str(port),'surface','--loopback-test'],
                    env={**os.environ,'XDG_CONFIG_HOME':tmp,'WINHOMEDESK_PROBE_GRAPHICS':str(client_mode)},
                    input=secret+'\n',text=True,capture_output=True,timeout=25)
                logs=log.read_text()
                passed=p.returncode==0 and 'CODEC name='+codec in logs
                if encoder:passed=passed and ('api=videotoolbox' in logs if encoder=='videotoolbox' else 'backend=openh264' in logs)
                if codec!='bitmap':passed=passed and 'CLIENT graphics=gfx initialized=1' in p.stderr
                if name=='avc444_gpu_preference':passed=passed and 'api=videotoolbox' not in logs
                result=dict(test=name,passed=bool(passed),exit=p.returncode,codec=codec,
                    output=p.stdout.strip().splitlines()[-1] if p.stdout.strip() else '')
            finally:
                server.terminate()
                try:server.wait(timeout=8)
                except subprocess.TimeoutExpired:server.kill();server.wait()
        result['shutdown']=server.returncode
        result['passed']=result['passed'] and server.returncode==0
        diagnostics='\n'.join(line for line in log.read_text().splitlines() if any(x in line for x in ('CODEC','ENCODER','CAPTURE','GFX caps','ERROR')))
        result['diagnostics']=diagnostics
        (ROOT/f'output/macos/video-{name}.log').write_text(log.read_text()+'\nCLIENT\n'+p.stdout+p.stderr)
        results.append(result);print(json.dumps(result,ensure_ascii=False),flush=True)
(ROOT/'output/macos/video-matrix-results.json').write_text(json.dumps(results,indent=2,ensure_ascii=False))
raise SystemExit(0 if all(r['passed'] for r in results) else 1)

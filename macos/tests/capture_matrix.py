#!/usr/bin/env python3
"""Capture the authorized local screen to loopback only; never save screen pixels."""
import ctypes, hashlib, json, os, pathlib, re, secrets, socket, subprocess, tempfile, time
ROOT=pathlib.Path(__file__).resolve().parents[2]
MAC=ROOT/'output/macos/WinHomeDesk.app/Contents/MacOS'
common=ctypes.CDLL('/usr/lib/system/libcommonCrypto.dylib')
common.CC_MD4.argtypes=[ctypes.c_void_p,ctypes.c_uint32,ctypes.c_void_p]
results=[]
for capture in ('auto','screenshot'):
 for edge,fps in ((1920,20),(1440,30),(0,45)):
    name=f'{capture}-{edge}-{fps}'
    with tempfile.TemporaryDirectory(prefix='whd-capture-') as tmp:
        secret=secrets.token_urlsafe(18);salt=secrets.token_bytes(16)
        nt=(ctypes.c_ubyte*16)();raw=secret.encode('utf-16le');common.CC_MD4(raw,len(raw),nt)
        with socket.socket() as sock:sock.bind(('127.0.0.1',0));port=sock.getsockname()[1]
        env={**os.environ,'XDG_CONFIG_HOME':tmp,'SURFACE_DESK_DATA':tmp,
             'SURFACE_DESK_SALT':salt.hex(),'SURFACE_DESK_HASH':hashlib.pbkdf2_hmac('sha1',secret.encode(),salt,200000,32).hex(),
             'WINHOME_DESK_NT_HASH':bytes(nt).hex(),'SURFACE_DESK_GPU':'1','SURFACE_DESK_FPS':str(fps),
             'SURFACE_DESK_MAX_EDGE':str(edge),'SURFACE_DESK_CAPTURE':capture}
        env.pop('WINHOMEDESK_TEST_PATTERN',None)
        log=pathlib.Path(tmp)/'server.log'
        with log.open('w') as out:
            server=subprocess.Popen([str(MAC/'WinHomeDeskServer'),f'/port:{port}','/bind-address:127.0.0.1',
                '+gfx','+gfx-planar','+gfx-avc420','-gfx-avc444','-gfx-progressive','-rfx','-nsc','+bitmap-compat'],env=env,stdout=out,stderr=out)
            try:
                deadline=time.monotonic()+15
                while 'CAPTURE first_frame=' not in log.read_text() and time.monotonic()<deadline and server.poll() is None:time.sleep(.1)
                p=subprocess.run([str(MAC/'WinHomeDesk'),'--probe','127.0.0.1',str(port),'surface','--capture-test'],
                    env={**os.environ,'XDG_CONFIG_HOME':tmp,'WINHOMEDESK_PROBE_GRAPHICS':'0'},
                    input=secret+'\n',text=True,capture_output=True,timeout=25)
                logs=log.read_text()
                match=re.search(r'source=(\d+)x(\d+) stream=(\d+)x(\d+) fps=(\d+)',logs)
                dimensions=bool(match)
                if match:
                    nw,nh,w,h,rate=map(int,match.groups());scale=min(1,edge/max(nw,nh)) if edge else 1
                    dimensions=(w,h,rate)==((int(nw*scale)+3)&~3,(int(nh*scale)+3)&~3,fps)
                    dimensions=dimensions and f'first_frame={w}x{h}' in logs
                expected='ScreenCaptureKit' if capture=='auto' else 'SCScreenshotManager'
                result=dict(test=name,passed=p.returncode==0 and dimensions and 'backend='+expected in logs and 'api=videotoolbox' in logs and 'CODEC name=avc420' in logs,
                    exit=p.returncode,output=p.stdout.strip().splitlines()[-1] if p.stdout.strip() else '')
            finally:
                server.terminate()
                try:server.wait(timeout=8)
                except subprocess.TimeoutExpired:server.kill();server.wait()
        result['shutdown']=server.returncode;result['passed']=result['passed'] and server.returncode==0
        result['diagnostics']='\n'.join(l for l in log.read_text().splitlines() if any(x in l for x in ('CAPTURE','ENCODER','CODEC','PERF_SEND')))
        (ROOT/f'output/macos/capture-{name}.log').write_text(log.read_text()+'\nCLIENT\n'+p.stdout+p.stderr)
        results.append(result);print(json.dumps(result,ensure_ascii=False),flush=True)
(ROOT/'output/macos/capture-matrix-results.json').write_text(json.dumps(results,ensure_ascii=False,indent=2))
raise SystemExit(0 if all(r['passed'] for r in results) else 1)

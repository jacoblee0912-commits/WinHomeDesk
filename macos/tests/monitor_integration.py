#!/usr/bin/env python3
"""Two real RDP monitors: negative X/Y, GFX/bitmap, per-monitor views and wire input."""
import ctypes,hashlib,json,os,pathlib,re,secrets,socket,subprocess,tempfile,time
ROOT=pathlib.Path(__file__).resolve().parents[2]
APP=pathlib.Path(os.environ.get('WINHOMEDESK_TEST_APP',ROOT/'output/macos/WinHomeDesk.app'))
MAC=APP/'Contents/MacOS'
common=ctypes.CDLL('/usr/lib/system/libcommonCrypto.dylib');common.CC_MD4.argtypes=[ctypes.c_void_p,ctypes.c_uint32,ctypes.c_void_p]
results=[]
for layout,mode in [('horizontal',0),('vertical',0),('horizontal',3),('vertical',3)]:
    name=f'{layout}-{mode}'
    with tempfile.TemporaryDirectory(prefix='whd-monitors-') as tmp:
        secret=secrets.token_urlsafe(18);salt=secrets.token_bytes(16);nt=(ctypes.c_ubyte*16)();raw=secret.encode('utf-16le');common.CC_MD4(raw,len(raw),nt)
        with socket.socket() as sock:sock.bind(('127.0.0.1',0));port=sock.getsockname()[1]
        env={**os.environ,'XDG_CONFIG_HOME':tmp,'SURFACE_DESK_DATA':tmp,'SURFACE_DESK_SALT':salt.hex(),
             'SURFACE_DESK_HASH':hashlib.pbkdf2_hmac('sha1',secret.encode(),salt,200000,32).hex(),'WINHOME_DESK_NT_HASH':bytes(nt).hex(),
             'WINHOMEDESK_TEST_PATTERN':'1','WINHOMEDESK_TEST_MONITORS':layout,'SURFACE_DESK_GPU':'1'}
        log=pathlib.Path(tmp)/'server.log'
        with log.open('w') as out:
            args=['-gfx'] if mode==3 else ['+gfx','+gfx-avc420','-gfx-avc444']
            server=subprocess.Popen([str(MAC/'WinHomeDeskServer'),f'/port:{port}','/bind-address:127.0.0.1','/max-connections:1','-rfx','-nsc',*args],env=env,stdout=out,stderr=out)
            try:
                deadline=time.monotonic()+15
                while 'test-pattern-no-input' not in log.read_text() and time.monotonic()<deadline and server.poll() is None:time.sleep(.1)
                client=subprocess.run([str(ROOT/'output/macos/TestMonitorViews'),str(port),str(mode),layout],input=secret+'\n',text=True,capture_output=True,timeout=20,env={**os.environ,'XDG_CONFIG_HOME':tmp})
                # Coordinates must reach the server, not just the view's local helper.
                expected=[(600,300),(200,300)] if layout=='horizontal' else [(400,450),(400,150)]
                wire=all(f'mouse x={x} y={y} flags={flags}' in log.read_text() for x,y in expected for flags in (36864,4096))
                wire=wire and 'mouse x=0 y=0' not in log.read_text()
                result=dict(test=name,passed=client.returncode==0 and wire,exit=client.returncode,wire_input=wire)
            finally:
                server.terminate()
                try:server.wait(timeout=8)
                except subprocess.TimeoutExpired:server.kill();server.wait()
        result['shutdown']=server.returncode;result['passed']=result['passed'] and server.returncode==0
        (ROOT/f'output/macos/monitors-{name}.log').write_text(log.read_text()+'\nCLIENT\n'+client.stdout+client.stderr)
        results.append(result);print(json.dumps(result),flush=True)
(ROOT/'output/macos/monitor-results.json').write_text(json.dumps(results,indent=2))
raise SystemExit(0 if all(r['passed'] for r in results) else 1)

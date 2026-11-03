#!/usr/bin/env python3
"""Loopback-only authenticated RDP with synthetic pixels; never injects OS input."""
import ctypes, hashlib, os, pathlib, secrets, socket, subprocess, tempfile, time, json, argparse
parser=argparse.ArgumentParser()
parser.add_argument('--live',action='store_true',help='Read real screen via ScreenCaptureKit; no input injection')
parser.add_argument('--input',action='store_true',help='Real capture and OS input in a dedicated local test window')
args=parser.parse_args()
input_test=args.input
live=args.live or input_test
ROOT=pathlib.Path(__file__).resolve().parents[2]
MAC=pathlib.Path(os.environ.get('WINHOMEDESK_TEST_APP',ROOT/'output/macos/WinHomeDesk.app'))/'Contents/MacOS'
common=ctypes.CDLL('/usr/lib/system/libcommonCrypto.dylib')
common.CC_MD4.argtypes=[ctypes.c_void_p,ctypes.c_uint32,ctypes.c_void_p]
password=secrets.token_urlsafe(18)
nt=(ctypes.c_ubyte*16)(); raw=password.encode('utf-16le'); common.CC_MD4(raw,len(raw),nt)
salt=secrets.token_bytes(16)
results=[]
with tempfile.TemporaryDirectory(prefix='whd-rdp-') as tmp:
    with socket.socket() as sock: sock.bind(('127.0.0.1',0)); port=sock.getsockname()[1]
    env=os.environ.copy(); env['XDG_CONFIG_HOME']=tmp; env.update(SURFACE_DESK_SALT=salt.hex(),
        SURFACE_DESK_HASH=hashlib.pbkdf2_hmac('sha1',password.encode(),salt,200000,dklen=32).hex(),
        WINHOME_DESK_NT_HASH=bytes(nt).hex(),SURFACE_DESK_DATA=tmp,WINHOMEDESK_TEST_PATTERN='1')
    if live: env.pop('WINHOMEDESK_TEST_PATTERN')
    log=pathlib.Path(tmp)/'server.log'
    with log.open('w') as output:
        server=subprocess.Popen([str(MAC/'WinHomeDeskServer'),f'/port:{port}','/bind-address:127.0.0.1','-gfx','-rfx','-nsc','+bitmap-compat'],env=env,stdout=output,stderr=output)
        try:
            deadline=time.monotonic()+15
            while time.monotonic()<deadline:
                if server.poll() is not None: raise RuntimeError('Server exited: '+log.read_text())
                if ('CAPTURE first_frame=' if live else 'test-pattern-no-input') in log.read_text(): break
                time.sleep(.1)
            else: raise RuntimeError('Server startup timed out: '+log.read_text())
            cases=[('capture_live',password,0)] if live else [('nla_valid',password,0),('nla_wrong_password','wrong-password',1),('nla_reconnect',password,0),('tls_valid',password,0),('tls_wrong_password','wrong-password',1)]
            for label, secret, expected in cases:
                extra=['--tls-test'] if label.startswith('tls_') else []
                command=[str(ROOT/'output/macos/InputTest'),str(port)] if input_test else [str(MAC/'WinHomeDesk'),'--probe','127.0.0.1',str(port),'surface','--capture-test' if live else '--loopback-test']+extra
                if input_test: label='live_os_input'
                p=subprocess.run(command,input=secret+'\n',text=True,capture_output=True,timeout=25,env={**os.environ,'XDG_CONFIG_HOME':tmp})
                result=dict(test=label,passed=(p.returncode==expected),exit=p.returncode,output=p.stdout.strip())
                results.append(result)
                print(json.dumps(result,ensure_ascii=False))
                if p.returncode!=expected: print(p.stderr[-6000:]); print(log.read_text()[-6000:])
        finally:
            server.terminate()
            try: server.wait(timeout=8)
            except subprocess.TimeoutExpired:
                server.kill(); server.wait(); results.append(dict(test='server_shutdown',passed=False))
    results.append(dict(test='server_shutdown',passed=server.returncode==0,exit=server.returncode))
    for event in (() if live else ('keyboard','unicode','mouse')):
        results.append(dict(test='input_protocol_'+event,passed=('TEST_INPUT '+event) in log.read_text()))
    if not live:
        # Three authenticated connections, each with one press/repeat/release
        # and one release-only Hangul event. This also exercises the server's
        # main-thread dispatch without selecting a real system input source.
        toggles=log.read_text().count('TEST_INPUT ime_toggle')
        results.append(dict(test='input_protocol_hangul',passed=toggles==6,toggles=toggles))
    # Keep diagnostics, never credentials or certificates, in ignored output directory.
    prefix='input' if input_test else 'live' if live else 'integration'
    (ROOT/f'output/macos/{prefix}-server.log').write_text(log.read_text())
(ROOT/f'output/macos/{prefix}-results.json').write_text(json.dumps(results,indent=2,ensure_ascii=False))
raise SystemExit(0 if all(r['passed'] for r in results) else 1)

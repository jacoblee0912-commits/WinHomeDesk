#!/usr/bin/env python3
"""Synthetic PCM over rdpsnd + simultaneous windows; loopback, no OS input."""
import ctypes, hashlib, json, os, pathlib, secrets, socket, subprocess, tempfile, time, re
ROOT=pathlib.Path(__file__).resolve().parents[2]
MAC=pathlib.Path(os.environ.get('WINHOMEDESK_TEST_APP',ROOT/'output/macos/WinHomeDesk.app'))/'Contents/MacOS'
common=ctypes.CDLL('/usr/lib/system/libcommonCrypto.dylib')
common.CC_MD4.argtypes=[ctypes.c_void_p,ctypes.c_uint32,ctypes.c_void_p]
secret=secrets.token_urlsafe(18);nt=(ctypes.c_ubyte*16)();raw=secret.encode('utf-16le');common.CC_MD4(raw,len(raw),nt)
results=[]
with tempfile.TemporaryDirectory(prefix='whd-media-') as tmp:
    servers=[];files=[];logs=[];ports=[]
    try:
        for index in range(2):
            folder=pathlib.Path(tmp)/str(index);folder.mkdir();salt=secrets.token_bytes(16)
            with socket.socket() as sock:sock.bind(('127.0.0.1',0));port=sock.getsockname()[1]
            ports.append(port);log=folder/'server.log';logs.append(log);out=log.open('w');files.append(out)
            env={**os.environ,'XDG_CONFIG_HOME':str(folder),'SURFACE_DESK_DATA':str(folder),
                 'SURFACE_DESK_SALT':salt.hex(),'SURFACE_DESK_HASH':hashlib.pbkdf2_hmac('sha1',secret.encode(),salt,200000,32).hex(),
                 'WINHOME_DESK_NT_HASH':bytes(nt).hex(),'WINHOMEDESK_TEST_PATTERN':'1','SURFACE_DESK_AUDIO':'1',
                 'SURFACE_DESK_GPU':'1'}
            server=subprocess.Popen([str(MAC/'WinHomeDeskServer'),f'/port:{port}','/bind-address:127.0.0.1',
                '/max-connections:1','+gfx','+gfx-avc420','-gfx-avc444','-rfx','-nsc'],env=env,stdout=out,stderr=out);servers.append(server)
            deadline=time.monotonic()+15
            while 'test-pattern-no-input' not in log.read_text() and server.poll() is None and time.monotonic()<deadline:time.sleep(.1)
            assert server.poll() is None and 'test-pattern-no-input' in log.read_text(),log.read_text()[-3000:]
        p=subprocess.run([str(MAC/'WinHomeDesk'),'--probe','127.0.0.1',str(ports[0]),'surface','--loopback-test'],
            input=secret+'\n',text=True,capture_output=True,timeout=25,
            env={**os.environ,'XDG_CONFIG_HOME':tmp,'WINHOMEDESK_PROBE_GRAPHICS':'0',
                 'WINHOMEDESK_PROBE_AUDIO':'1','SURFACE_PROBE_AUDIO_STATS':'1'})
        audio=re.search(r'PROBE_AUDIO bytes=(\d+) nonzero=(\d+) peak=(\d+)',p.stdout)
        packets=re.search(r'PROBE_AUDIO_PACKETS count=(\d+) min_bytes=(\d+) max_bytes=(\d+)',p.stdout)
        passed=(p.returncode==0 and audio and int(audio[2])>1000 and packets and int(packets[1])>=10 and
                int(packets[2])==1920 and int(packets[3])==1920 and 'rate=48000 channels=2 bits=16' in p.stdout and
                'AUDIO_CHANNEL state=ready' in logs[0].read_text())
        results.append(dict(test='rdpsnd_synthetic_pcm',passed=bool(passed),output=p.stdout.strip()))
        (ROOT/'output/macos/media-audio-client.log').write_text(p.stdout+p.stderr)
        # Ensure the previous controller has left before opening both windows.
        time.sleep(.3)
        p=subprocess.run([str(ROOT/'output/macos/TestMultiWindow'),*(str(x) for x in ports)],input=secret+'\n',
            text=True,capture_output=True,timeout=20,env={**os.environ,'XDG_CONFIG_HOME':tmp})
        results.append(dict(test='multiwindow_rdp',passed=p.returncode==0,output=p.stdout.strip()))
        (ROOT/'output/macos/media-multiwindow-client.log').write_text(p.stdout+p.stderr)
    finally:
        for s in servers:s.terminate()
        for i,s in enumerate(servers):
            try:s.wait(timeout=8)
            except subprocess.TimeoutExpired:s.kill();s.wait()
            files[i].close()
            results.append(dict(test=f'shutdown_{i}',passed=s.returncode==0))
            (ROOT/f'output/macos/media-server-{i}.log').write_text(logs[i].read_text())
for result in results:print(json.dumps(result,ensure_ascii=False))
(ROOT/'output/macos/media-results.json').write_text(json.dumps(results,ensure_ascii=False,indent=2))
raise SystemExit(0 if all(r['passed'] for r in results) else 1)

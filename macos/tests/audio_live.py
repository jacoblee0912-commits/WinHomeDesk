#!/usr/bin/env python3
"""Real system playback -> SCK -> rdpsnd -> fake sink. Never saves captured audio."""
import ctypes, hashlib, json, math, os, pathlib, re, secrets, socket, struct, subprocess, tempfile, time, wave
ROOT=pathlib.Path(__file__).resolve().parents[2];MAC=ROOT/'output/macos/WinHomeDesk.app/Contents/MacOS'
common=ctypes.CDLL('/usr/lib/system/libcommonCrypto.dylib');common.CC_MD4.argtypes=[ctypes.c_void_p,ctypes.c_uint32,ctypes.c_void_p]
results=[]
for mode in ('auto','screenshot'):
    with tempfile.TemporaryDirectory(prefix='whd-live-audio-') as tmp:
        secret=secrets.token_urlsafe(18);salt=secrets.token_bytes(16)
        nt=(ctypes.c_ubyte*16)();raw=secret.encode('utf-16le');common.CC_MD4(raw,len(raw),nt)
        tone=pathlib.Path(tmp)/'synthetic-tone.wav'
        with wave.open(str(tone),'wb') as w:
            w.setparams((2,2,48000,0,'NONE','not compressed'))
            w.writeframes(b''.join(struct.pack('<hh',*(2*[int(6000*min(1,i/2400,(96000-i)/2400)*math.sin(2*math.pi*440*i/48000))])) for i in range(96000)))
        with socket.socket() as sock:sock.bind(('127.0.0.1',0));port=sock.getsockname()[1]
        env={**os.environ,'XDG_CONFIG_HOME':tmp,'SURFACE_DESK_DATA':tmp,'SURFACE_DESK_SALT':salt.hex(),
             'SURFACE_DESK_HASH':hashlib.pbkdf2_hmac('sha1',secret.encode(),salt,200000,32).hex(),
             'WINHOME_DESK_NT_HASH':bytes(nt).hex(),'SURFACE_DESK_AUDIO':'1','SURFACE_DESK_GPU':'1','SURFACE_DESK_CAPTURE':mode}
        env.pop('WINHOMEDESK_TEST_PATTERN',None)
        log=pathlib.Path(tmp)/'server.log';play=None;probe=None
        with log.open('w') as out:
            server=subprocess.Popen([str(MAC/'WinHomeDeskServer'),f'/port:{port}','/bind-address:127.0.0.1',
                '+gfx','+gfx-avc420','-gfx-avc444','-rfx','-nsc'],env=env,stdout=out,stderr=out)
            try:
                deadline=time.monotonic()+15
                while 'CAPTURE first_frame=' not in log.read_text() and server.poll() is None and time.monotonic()<deadline:time.sleep(.1)
                probe=subprocess.Popen([str(MAC/'WinHomeDesk'),'--probe','127.0.0.1',str(port),'surface','--capture-test'],
                    stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,
                    env={**os.environ,'XDG_CONFIG_HOME':tmp,'WINHOMEDESK_PROBE_GRAPHICS':'0',
                         'WINHOMEDESK_PROBE_AUDIO':'1','SURFACE_PROBE_AUDIO_STATS':'1'})
                probe.stdin.write(secret+'\n');probe.stdin.flush()
                deadline=time.monotonic()+10
                while 'AUDIO_CHANNEL state=ready' not in log.read_text() and probe.poll() is None and time.monotonic()<deadline:time.sleep(.05)
                play=subprocess.Popen(['/usr/bin/afplay','-v','0.1',str(tone)],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
                stdout,stderr=probe.communicate(timeout=20)
                audio=re.search(r'PROBE_AUDIO bytes=(\d+) nonzero=(\d+) peak=(\d+)',stdout)
                packets=re.search(r'PROBE_AUDIO_PACKETS count=(\d+) min_bytes=(\d+) max_bytes=(\d+)',stdout)
                passed=probe.returncode==0 and audio and int(audio[2])>1000 and packets and int(packets[2])==1920 and int(packets[3])==1920
                result=dict(test='system_audio_'+mode,passed=bool(passed),output=stdout.strip())
                (ROOT/f'output/macos/audio-live-{mode}-client.log').write_text(stdout+stderr)
            finally:
                if play and play.poll() is None:play.terminate();play.wait()
                if probe and probe.poll() is None:probe.kill();probe.wait()
                server.terminate()
                try:server.wait(timeout=8)
                except subprocess.TimeoutExpired:server.kill();server.wait()
        result['shutdown']=server.returncode;result['passed']=result['passed'] and server.returncode==0
        results.append(result)
        (ROOT/f'output/macos/audio-live-{mode}-server.log').write_text(log.read_text())
        print(json.dumps(result,ensure_ascii=False),flush=True)
(ROOT/'output/macos/audio-live-results.json').write_text(json.dumps(results,ensure_ascii=False,indent=2))
raise SystemExit(0 if all(r['passed'] for r in results) else 1)

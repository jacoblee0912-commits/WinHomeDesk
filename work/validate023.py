from pathlib import Path
import subprocess,os,json,re
w=Path.cwd()/'work';env=os.environ.copy();env['PATH']=str(w/'compiler/llvm-mingw-20261020-ucrt-x86_64/bin')+';'+env['PATH']
def run(name,args,timeout=180):
 p=subprocess.run(args,env=env,capture_output=True,timeout=timeout);text=p.stdout.decode(errors='replace')+p.stderr.decode(errors='replace')
 (w/(name+'.log')).write_text(text,encoding='utf-8');assert p.returncode==0,text[-2500:];print(name,'PASS',flush=True)
run('audio022-build',[str(w/'tooling/cmake/data/bin/cmake.exe'),'--build',str(w/'build-ready'),'--target','surface-audio-timing-probe','surface-audio-probe','--parallel','6'])
run('audio022-timing',[str(w/'build-ready/surface-audio-timing-probe.exe')])
csc=Path(os.environ['WINDIR'])/'Microsoft.NET/Framework64/v4.0.30319/csc.exe'
run('audio022-ui-build',[str(csc),'/nologo','/platform:x64','/out:'+str(w/'TestAudioCaption022.exe'),'/reference:'+str(w/'SurfaceDesk.exe'),'/reference:System.Windows.Forms.dll',str(w/'test_AudioCaption022.cs')])
run('audio022-ui',[str(w/'TestAudioCaption022.exe')])
run('audio023-integration',['python',str(w/'test_audio023.py')],120)
env['SURFACE_DESK_GPU']='1'
run('graphics023-gpu',['python',str(w/'test_graphics020.py'),'avc420'])
run('graphics023-microsoft',['python',str(w/'test_graphics020.py'),'microsoft'])
run('gui023',['python',str(w/'test_gui020.py')])
print('0.2.3 audio tests and targeted regression PASS',flush=True)

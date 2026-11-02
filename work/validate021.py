from pathlib import Path
import subprocess,os,json,re,shutil
w=Path.cwd()/'work';results={};env=os.environ.copy()
env['PATH']=str(w/'compiler/llvm-mingw-20261020-ucrt-x86_64/bin')+';'+env['PATH']
def run(name,command,extra=None,timeout=180):
 e=env.copy();e.update(extra or {})
 p=subprocess.run(command,env=e,capture_output=True,timeout=timeout)
 text=p.stdout.decode(errors='replace')+p.stderr.decode(errors='replace')
 (w/(name+'.log')).write_text(text,encoding='utf-8');assert p.returncode==0,name+': '+text[-2000:]
 print(name,'PASS',flush=True);return text
run('worker021-build',[str(w/'tooling/cmake/data/bin/cmake.exe'),'--build',str(w/'build-ready'),'--target','surface-worker-probe','--parallel','6'])
run('worker021',[str(w/'build-ready/surface-worker-probe.exe')])
for name,gpu,sta in [('cpu','0',''),('gpu','1',''),('fallback','1','1')]:
 e={'SURFACE_DESK_GPU':gpu}
 if sta:e['SURFACE_PROBE_COM_STA']=sta
 text=run('codec021-'+name,[str(w/'runtime/surface-codec-probe.exe')],e)
 if name=='gpu':assert 'backend=gpu' in text and 'reason=gpu-' not in text
 if name=='fallback':assert 'reason=gpu-init-failed hr=0x80010106' in text and 'backend=gpu' not in text
 results[name]=[a for a in text.splitlines() if 'PASS' in a or 'ENCODER backend=' in a or 'GPU_DEVICE' in a]
bench=[]
for repeat in range(3):
 for mode in ['cpu','gpu']:
  text=run('bench021-'+mode+str(repeat),[str(w/'runtime/surface-benchmark.exe')],{'SURFACE_DESK_GPU':'1' if mode=='gpu' else '0'})
  line=next(a for a in text.splitlines() if a.startswith('BENCH '));row={k:float(v) for k,v in (a.split('=') for a in line.split()[1:])}
  if mode=='gpu':assert 'backend=gpu' in text and 'reason=gpu-' not in text
  bench.append(dict(mode=mode,repeat=repeat,**row))
(w/'bench021-results.json').write_text(json.dumps(bench,indent=2))
run('gui021',['python',str(w/'test_gui020.py')])
run('rdg021',['python',str(w/'build_tests018.py')])
run('regression021',['python',str(w/'regression020.py')],{'SURFACE_DESK_GPU':'1'},timeout=300)
for prefix in ['graphics','audio','auth']:shutil.copy2(w/(prefix+'020-results.json'),w/(prefix+'021-results.json'))
run('cpu021-connection',['python',str(w/'test_graphics020.py'),'avc420'],{'SURFACE_DESK_GPU':'0','SURFACE_PROBE_MOUSE':'1'})
text=(w/'gfx020-avc420.log').read_text(errors='replace');assert 'during_encode=' in text and 'backend=gpu' not in text
(w/'cpu021-server.log').write_text(text,encoding='utf-8')
run('gpu021-connection',['python',str(w/'test_graphics020.py'),'avc420'],{'SURFACE_DESK_GPU':'1','SURFACE_PROBE_MOUSE':'1'})
text=(w/'gfx020-avc420.log').read_text(errors='replace');assert 'during_encode=' in text and 'backend=gpu' in text
(w/'gpu021-server.log').write_text(text,encoding='utf-8')
(w/'validation021-results.json').write_text(json.dumps(results,ensure_ascii=False,indent=2),encoding='utf-8')
print('ALL 0.2.1 CHECKS PASS',flush=True)

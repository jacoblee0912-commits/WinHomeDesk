from pathlib import Path
import subprocess,os,zipfile,shutil,json,re
w=Path.cwd()/'work';old=w/'baseline020';old.mkdir(exist_ok=True)
with zipfile.ZipFile(Path.cwd()/'outputs/SurfaceDesk-0.1.9-Windows-x64.zip') as z:
 for n in z.namelist():
  if '/engine/' in n and n.endswith('.dll') and '/ossl-modules/' not in n:
   (old/Path(n).name).write_bytes(z.read(n))
shutil.copy2(w/'runtime/surface-benchmark.exe',old/'surface-benchmark.exe')
results=[]
for repetition in range(3):
 for label,folder,profile in [('0.1.9',old,'legacy'),('0.2.0',w/'runtime','fast')]:
  env=os.environ.copy();env['SURFACE_DESK_ENCODER_PROFILE']=profile
  p=subprocess.run([str(folder/'surface-benchmark.exe')],env=env,cwd=folder,capture_output=True,timeout=60)
  text=p.stdout.decode(errors='replace')+p.stderr.decode(errors='replace')
  (w/f'bench020-{label}-{repetition}.log').write_text(text,encoding='utf-8')
  assert p.returncode==0,text[-2000:]
  line=next(x for x in text.splitlines() if x.startswith('BENCH '));row=dict(re.findall(r'(\w+)=([\d.]+)',line))
  results.append(dict(version=label,run=repetition,**{k:float(v) for k,v in row.items()}));print(label,repetition,line,flush=True)
(w/'bench020-results.json').write_text(json.dumps(results,indent=2),encoding='utf-8')

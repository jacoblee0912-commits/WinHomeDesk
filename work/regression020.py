from pathlib import Path
import subprocess,sys
w=Path.cwd()/'work'
for name in ['test_audio020.py','test_graphics020.py','integration020.py']:
 with (w/(name+'.runlog')).open('w',encoding='utf-8') as log:
  p=subprocess.run([sys.executable,str(w/name)],stdout=log,stderr=subprocess.STDOUT)
 if p.returncode:raise RuntimeError(name+' failed: '+(w/(name+'.runlog')).read_text(encoding='utf-8',errors='replace')[-3000:])
 print(name,'PASS',flush=True)

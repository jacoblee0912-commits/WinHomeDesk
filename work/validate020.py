from pathlib import Path
import subprocess
w=Path.cwd()/'work'
for name,exe in [('pipeline',w/'build-ready/surface-pipeline-probe.exe'),('pacer',w/'build-ready/surface-pacer-probe.exe'),('codec',w/'runtime/surface-codec-probe.exe')]:
 p=subprocess.run([str(exe)],cwd=exe.parent,capture_output=True,timeout=60)
 text=p.stdout.decode(errors='replace')+p.stderr.decode(errors='replace')
 (w/(name+'020-results.txt')).write_text(text,encoding='utf-8')
 assert p.returncode==0,text[-3000:]
 print(name,text[-1300:],flush=True)

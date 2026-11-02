from pathlib import Path
import subprocess,os
w=Path.cwd()/'work';csc=Path(os.environ['WINDIR'])/'Microsoft.NET/Framework64/v4.0.30319/csc.exe'
def compile(name,files,refs):
 subprocess.run([str(csc),'/nologo','/platform:x64','/out:'+str(w/name),*['/reference:'+r for r in refs],*[str(w/f) for f in files]],check=True)
compile('TestDesktopOptions.exe',['TestDesktopOptions.cs','DesktopOptions.cs'],['System.Security.dll','System.Xml.dll'])
compile('TestLifecycle018.exe',['TestLifecycle017.cs'],[str(w/'WinHomeDesk.exe'),'System.Windows.Forms.dll','System.Drawing.dll'])
compile('TestPerformanceMetrics.exe',['TestPerformanceMetrics.cs','PerformanceMetrics.cs'],[])
results=[]
for name,args in [('TestDesktopOptions.exe',[str(Path.cwd())]),('TestPerformanceMetrics.exe',[]),('TestLifecycle018.exe',[])]:
 p=subprocess.run([str(w/name),*args],capture_output=True,timeout=35)
 text=p.stdout.decode(errors='replace')+p.stderr.decode(errors='replace');print(text.strip())
 if p.returncode:raise RuntimeError(name+' failed '+str(p.returncode))
 results.append(text)
(w/'gui024-tests.txt').write_text('\n'.join(results),encoding='utf-8')

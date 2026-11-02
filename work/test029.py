from pathlib import Path
import subprocess,os
w=Path(__file__).resolve().parent
csc=Path(os.environ['WINDIR'])/'Microsoft.NET/Framework64/v4.0.30319/csc.exe'
results=[]
for name in ['TestDesktopOptions','TestLifecycle017','TestStartup029']:
 command=[str(csc),'/nologo','/platform:x64','/out:'+str(w/(name+'.exe')),'/reference:'+str(w/'WinHomeDesk.exe'),'/reference:Microsoft.CSharp.dll','/reference:System.Windows.Forms.dll','/reference:System.Drawing.dll','/reference:System.Security.dll',str(w/(name+'.cs'))]
 subprocess.run(command,check=True)
 result=subprocess.run([str(w/(name+'.exe')),str(w if name=='TestStartup029' else w.parent)],capture_output=True,timeout=45)
 text=result.stdout.decode('utf-8',errors='replace')+result.stderr.decode('utf-8',errors='replace')
 print(text,flush=True);results.append(name+'\n'+text)
 (w/'startup029-tests.txt').write_text('\n'.join(results),encoding='utf-8')
 assert result.returncode==0,(name,result.returncode)

from pathlib import Path
import subprocess,os
r=Path.cwd();w=r/'work';csc=Path(os.environ['WINDIR'])/'Microsoft.NET/Framework64/v4.0.30319/csc.exe'
files=['SurfaceDesk.cs','RdgManager.cs','PerformanceMetrics.cs','DesktopOptions.cs','SurfaceDesk.Lifecycle.cs','GraphicsMode.cs','AudioMode.cs','EncoderMode.cs']
subprocess.run([str(csc),'/nologo','/target:winexe','/platform:x64','/out:'+str(w/'WinHomeDesk.exe'),'/win32icon:'+str(w/'SurfaceDesk.ico'),'/reference:System.Windows.Forms.dll','/reference:System.Drawing.dll','/reference:System.Xml.dll','/reference:System.Security.dll',*[str(w/f) for f in files]],check=True)
subprocess.run([str(w/'WinHomeDesk.exe'),'--preview-perf',str(w/'preview-perf024.png')],check=True,timeout=20)
print('0.2.4 GUI built and preview rendered')

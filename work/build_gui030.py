from pathlib import Path
import subprocess,os
r=Path.cwd();w=r/'work';csc=Path(os.environ['WINDIR'])/'Microsoft.NET/Framework64/v4.0.30319/csc.exe'
files=['SurfaceDesk.cs','MonitorSharing.cs','SeparateMonitors.cs','RdgManager.cs','PerformanceMetrics.cs','DesktopOptions.cs','StartupRegistration.cs','Localization.cs','SurfaceDesk.Lifecycle.cs','GraphicsMode.cs','AudioMode.cs','EncoderMode.cs','AppCredentials.cs','RdcManClient.cs']
subprocess.run([str(csc),'/nologo','/target:winexe','/platform:x64','/out:'+str(w/'WinHomeDesk.exe'),'/win32icon:'+str(w/'SurfaceDesk.ico'),'/resource:'+str(w/'ime-fix/WinHomeDesk.ImeHook.dll')+',WinHomeDesk.ImeHook.dll','/resource:'+str(w/'Translations.xml')+',WinHomeDesk.Translations.xml','/reference:System.Windows.Forms.dll','/reference:System.Drawing.dll','/reference:System.Xml.dll','/reference:System.Security.dll','/reference:Microsoft.CSharp.dll',*[str(w/f) for f in files]],check=True)
subprocess.run([str(w/'WinHomeDesk.exe'),'--preview-perf',str(w/'preview-perf030.png')],check=True,timeout=20)
print('Windows GUI built with embedded IME hook')

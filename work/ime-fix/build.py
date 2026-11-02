from pathlib import Path
import subprocess,os
w=Path(__file__).resolve().parent
subprocess.run(['pwsh','-NoProfile','-File',str(w/'build.ps1')],check=True)
csc=Path(os.environ['WINDIR'])/'Microsoft.NET/Framework64/v4.0.30319/csc.exe'
icon=w.parent/'SurfaceDesk.ico'
if not icon.exists():icon=w/'WinHomeDesk.ico'
subprocess.run([str(csc),'/nologo','/target:winexe','/platform:x64','/reference:System.Windows.Forms.dll','/win32icon:'+str(icon),'/out:'+str(w/'WinHomeDesk-RDCMan-IME.exe'),str(w/'Launcher.cs')],check=True)

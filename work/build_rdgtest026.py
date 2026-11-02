from pathlib import Path
import subprocess,os
w=Path(__file__).resolve().parent;csc=Path(os.environ['WINDIR'])/'Microsoft.NET/Framework64/v4.0.30319/csc.exe'
subprocess.run([str(csc),'/nologo','/out:'+str(w/'TestRdg.exe'),'/reference:System.Windows.Forms.dll','/reference:System.Drawing.dll',str(w/'TestRdg.cs'),str(w/'RdgManager.cs')],check=True)
subprocess.run([str(w/'TestRdg.exe'),str(w.parent)],check=True)

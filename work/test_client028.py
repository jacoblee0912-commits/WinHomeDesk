from pathlib import Path
import subprocess,os
w=Path(__file__).resolve().parent;csc=Path(os.environ['WINDIR'])/'Microsoft.NET/Framework64/v4.0.30319/csc.exe'
subprocess.run([str(csc),'/nologo','/platform:x64','/out:'+str(w/'TestClient028.exe'),'/reference:'+str(w/'WinHomeDesk.exe'),'/reference:System.Windows.Forms.dll','/reference:System.Drawing.dll',str(w/'TestClient028.cs')],check=True)
p=subprocess.run([str(w/'TestClient028.exe'),str(w),os.environ['RDCMAN_EXE']],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=30)
(w/'client028-tests.txt').write_bytes(p.stdout);print(p.stdout.decode(errors='replace'));assert p.returncode==0

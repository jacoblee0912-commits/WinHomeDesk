from pathlib import Path
import subprocess,os
w=Path(__file__).resolve().parent;csc=Path(os.environ['WINDIR'])/'Microsoft.NET/Framework64/v4.0.30319/csc.exe'
subprocess.run([str(csc),'/nologo','/out:'+str(w/'TestCredentials026.exe'),str(w/'TestCredentials026.cs'),str(w/'AppCredentials.cs')],check=True)
p=subprocess.run([str(w/'TestCredentials026.exe')],capture_output=True,check=True)
(w/'credentials026-test.log').write_bytes(p.stdout);print(p.stdout.decode().strip())
subprocess.run(['python',str(w/'test_gui025.py')],check=True)

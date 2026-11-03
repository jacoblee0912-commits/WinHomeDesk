"""Separate-monitor regression tests. Local fake listeners only; no input injection."""
from pathlib import Path
import os,subprocess,argparse
p=argparse.ArgumentParser();p.add_argument('--compiler',required=True);a=p.parse_args()
w=Path(__file__).resolve().parent
csc=Path(os.environ['WINDIR'])/'Microsoft.NET/Framework64/v4.0.30319/csc.exe'
subprocess.run([str(csc),'/nologo','/platform:x64','/out:'+str(w/'TestSeparateMonitors032.exe'),'/reference:'+str(w/'WinHomeDesk.exe'),'/reference:System.Windows.Forms.dll','/reference:System.Drawing.dll',str(w/'TestSeparateMonitors032.cs')],check=True)
subprocess.run([a.compiler,str(w/'test_monitors032.c'),'-o',str(w/'test_monitors032.exe'),'-luser32'],check=True)
for cmd in [[str(w/'test_monitors032.exe')],[str(w/'TestSeparateMonitors032.exe'),str(w)]]:
 r=subprocess.run(cmd,capture_output=True,timeout=45);text=(r.stdout+r.stderr).decode('utf-8',errors='replace');print(text)
 if r.returncode:raise SystemExit(r.returncode)

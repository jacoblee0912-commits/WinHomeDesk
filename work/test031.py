"""GUI and real monitor-layout regression tests; never changes user settings.
Run from the repository root with --engine pointing to a complete staged engine.
"""
from pathlib import Path
import argparse, os, subprocess
p=argparse.ArgumentParser();p.add_argument('--engine',required=True);p.add_argument('--compiler',required=True);a=p.parse_args()
w=Path(__file__).resolve().parent
csc=Path(os.environ['WINDIR'])/'Microsoft.NET/Framework64/v4.0.30319/csc.exe'
subprocess.run([str(csc),'/nologo','/platform:x64','/out:'+str(w/'TestMonitorSharing031.exe'),'/reference:'+str(w/'WinHomeDesk.exe'),'/reference:System.Windows.Forms.dll','/reference:System.Drawing.dll',str(w/'TestMonitorSharing031.cs')],check=True)
subprocess.run([a.compiler,str(w/'test_monitors031.c'),'-o',str(w/'test_monitors031.exe'),'-luser32'],check=True)
reports=[]
for cmd in [[str(w/'test_monitors031.exe')],[str(w/'TestMonitorSharing031.exe'),str(w),str(Path(a.engine).resolve())]]:
 result=subprocess.run(cmd,capture_output=True,timeout=35)
 text=(result.stdout+result.stderr).decode('utf-8',errors='replace');reports.append(text);print(text)
 (w/'monitor031-tests.txt').write_text('\n'.join(reports),encoding='utf-8')
 if result.returncode:raise SystemExit(result.returncode)

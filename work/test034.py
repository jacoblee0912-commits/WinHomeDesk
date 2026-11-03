"""Clipboard regression: protocol/COM/file streams; OS clipboard only in a private station.
No user clipboard reads/writes, startup registrations, or input injection.
"""
from pathlib import Path
import argparse,os,subprocess,tempfile
p=argparse.ArgumentParser();p.add_argument('--compiler',required=True);p.add_argument('--runtime',required=True);a=p.parse_args()
w=Path(__file__).resolve().parent;build=w/'build';source=w/'FreeRDP';exe=w/'test_clipboard034.exe'
cmd=[a.compiler,str(w/'test_clipboard034.cpp'),'-o',str(exe),'-std=c++17','-DUNICODE','-D_UNICODE','-DNONAMELESSUNION','-DWIN32_LEAN_AND_MEAN','-D_WIN32_WINNT=0x0601']
cmd+=['-I'+str(x) for x in [source/'include',build/'include',source/'winpr/include',build/'winpr/include']]
cmd+=['-L'+str(x) for x in [build/'server/common',build/'libfreerdp',build/'winpr/libwinpr']]
cmd+=['-lfreerdp-server3','-lfreerdp3','-lwinpr3','-lole32','-lshell32','-luuid','-luser32','-static-libstdc++','-static-libgcc','-municode']
subprocess.run(cmd,check=True)
env=os.environ.copy();env['PATH']=str(Path(a.runtime).resolve())+os.pathsep+env['PATH']
with tempfile.TemporaryDirectory(prefix='whd-clipboard-',dir=w) as directory:
 result=subprocess.run([str(exe),directory],env=env,capture_output=True,timeout=35)
 text=(result.stdout+result.stderr).decode('utf-8',errors='replace');print(text)
 (w/'clipboard034-tests.txt').write_text(text,encoding='utf-8')
 if result.returncode:raise SystemExit(result.returncode)

csc=Path(os.environ['WINDIR'])/'Microsoft.NET/Framework64/v4.0.30319/csc.exe'
subprocess.run([str(csc),'/nologo','/platform:x64','/out:'+str(w/'TestClipboard034.exe'),'/reference:'+str(w/'WinHomeDesk.exe'),'/reference:System.Windows.Forms.dll','/reference:System.Drawing.dll',str(w/'TestClipboard034.cs')],check=True)
subprocess.run([str(w/'TestClipboard034.exe'),str(w)],check=True,timeout=30)

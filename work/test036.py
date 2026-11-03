from pathlib import Path
import argparse,os,subprocess
p=argparse.ArgumentParser();p.add_argument('--compiler',required=True);p.add_argument('--runtime',required=True);a=p.parse_args()
w=Path(__file__).resolve().parent;b=w/'build';src=w/'FreeRDP';reports=[]
for stem in ['test_clipboard036','test_clipboard_wire036']:
 cmd=[a.compiler,str(w/(stem+'.cpp')),'-o',str(w/(stem+'.exe')),'-std=c++17','-DUNICODE','-D_UNICODE','-DNONAMELESSUNION','-DWIN32_LEAN_AND_MEAN','-D_WIN32_WINNT=0x0601']
 cmd+=['-I'+str(x) for x in [src/'include',b/'include',src/'winpr/include',b/'winpr/include']]
 cmd+=['-L'+str(x) for x in [b/'server/common',b/'libfreerdp',b/'winpr/libwinpr']]
 cmd+=['-lfreerdp-server3','-lfreerdp3','-lwinpr3','-lole32','-lshell32','-luuid','-luser32','-static-libstdc++','-static-libgcc']
 subprocess.run(cmd,check=True)
 env=os.environ.copy();env['PATH']=str(Path(a.runtime).resolve())+os.pathsep+env['PATH']
 r=subprocess.run([str(w/(stem+'.exe'))],env=env,capture_output=True,timeout=25)
 text=(r.stdout+r.stderr).decode('utf-8',errors='replace');reports.append(text);print(text)
 if r.returncode:raise SystemExit(r.returncode)
(w/'clipboard036-tests.txt').write_text('\n'.join(reports),encoding='utf-8')

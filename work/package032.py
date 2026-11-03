"""Package freshly built Windows binaries, using the verified 0.3.0 ZIP for
third-party runtimes/licenses only. Never copies local credentials or settings.
"""
from pathlib import Path
import argparse, hashlib, json, os, re, shutil, subprocess, zipfile
import pefile
p=argparse.ArgumentParser();p.add_argument('--base',required=True);p.add_argument('--output',required=True);a=p.parse_args()
w=Path(__file__).resolve().parent;repo=w.parent;out=Path(a.output).resolve();out.mkdir(parents=True,exist_ok=True)
stage=out/'WinHomeDesk-0.3.2-local'
if stage.exists():raise SystemExit('Use an empty output directory; do not mix package versions.')
stage.mkdir();build=w/'build'
with zipfile.ZipFile(a.base) as base:
 for n in base.namelist():
  rel='/'.join(n.split('/')[1:])
  if not rel or n.endswith('/') or not (rel.startswith('engine/') or rel.startswith('licenses/') or rel=='WinHomeDesk.ico'):continue
  parts=Path(rel).parts
  if Path(rel).is_absolute() or '..' in parts:raise ValueError('Unsafe base archive path')
  dest=stage/rel;dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(base.read(n))
for old in list((stage/'engine').glob('libfreerdp*.dll'))+list((stage/'engine').glob('libwinpr*.dll')):
 matches=list(build.rglob(old.name))
 if len(matches)!=1:raise ValueError('Missing/ambiguous rebuilt DLL '+old.name)
 shutil.copy2(matches[0],old)
shutil.copy2(build/'server/shadow/cli/freerdp-shadow-cli.exe',stage/'engine/WinHomeDesk-engine.exe')
shutil.copy2(w/'WinHomeDesk.exe',stage/'WinHomeDesk.exe')
for language in ['ko','en']:
 shutil.copy2(repo/('docs/WINDOWS_MULTIMONITOR.'+language+'.md'),stage/('README.'+language+'.md'))
with (stage/'licenses/NOTICE.txt').open('a',encoding='utf-8') as notice:
 notice.write('\nWinHomeDesk 0.3.2-local: Windows multi-monitor canvas, native engine and GUI rebuilt from the included Windows source. Third-party runtime versions unchanged from the base 0.3.0 package.\n')
names={x.name.lower() for x in (stage/'engine').rglob('*.dll')}
audit=[]
for binary in (stage/'engine').rglob('*'):
 if binary.suffix.lower() not in ['.exe','.dll']:continue
 pe=pefile.PE(str(binary));assert pe.FILE_HEADER.Machine==0x8664
 imports=[x.dll.decode() for attr in ['DIRECTORY_ENTRY_IMPORT','DIRECTORY_ENTRY_DELAY_IMPORT'] for x in getattr(pe,attr,[])]
 for name in imports:
  if name.lower() in names or name.lower().startswith(('api-ms-','ext-ms-')):continue
  if not (Path(os.environ['WINDIR'])/'System32'/name).exists():raise ValueError('Unresolved import: '+binary.name+' -> '+name)
 audit.append({'file':str(binary.relative_to(stage)),'imports':imports});pe.close()
(out/'dependency-audit.json').write_text(json.dumps(audit,indent=2),encoding='utf-8')
for scope in ['0','1']:
 env=os.environ.copy();env.pop('WINHOME_DESK_MONITOR_DEVICE',None);env['PATH']=str(Path(os.environ['WINDIR'])/'System32');env.update(SURFACE_DESK_ALL_MONITORS=scope,SURFACE_DESK_MAX_EDGE='1920')
 run=subprocess.run([str(stage/'engine/WinHomeDesk-engine.exe'),'/monitors'],cwd=stage,env=env,capture_output=True,timeout=12)
 text=(run.stdout+run.stderr).decode('utf-8',errors='replace')
 assert run.returncode in [0,-2001,4294965295] and re.search(r'MONITOR_LIST count=[1-9]',text) and 'MONITOR_CANVAS protocol_monitors=1' in text,text
 (out/('package-monitors-'+scope+'.txt')).write_text(text,encoding='utf-8')
# Exercise every selectable output using the final packaged DLLs and a clean PATH.
devices=re.findall(r'MONITOR index=\d+ device=(DISPLAY\d+)',text)
assert devices
for device in devices:
 env['WINHOME_DESK_MONITOR_DEVICE']=device
 run=subprocess.run([str(stage/'engine/WinHomeDesk-engine.exe'),'/monitors'],cwd=stage,env=env,capture_output=True,timeout=12)
 selected=(run.stdout+run.stderr).decode('utf-8',errors='replace')
 assert run.returncode in [0,-2001,4294965295] and re.findall(r'MONITOR index=\d+ device=(DISPLAY\d+)',selected)==[device] and 'MONITOR_LIST count=1 ' in selected and 'MONITOR_SELECT version=1' in selected,selected
 (out/('package-'+device+'.txt')).write_text(selected,encoding='utf-8')
manifest={str(f.relative_to(stage)).replace('\\','/'):hashlib.sha256(f.read_bytes()).hexdigest() for f in stage.rglob('*') if f.is_file()}
(stage/'SHA256.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
target=out/'WinHomeDesk-0.3.2-local-Windows-x64.zip'
with zipfile.ZipFile(target,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
 for f in stage.rglob('*'):
  if f.is_file():z.write(f,f.relative_to(out))
with zipfile.ZipFile(target) as z:
 assert z.testzip() is None
 for name,sha in manifest.items():assert hashlib.sha256(z.read(stage.name+'/'+name)).hexdigest()==sha
print(target);print('SHA256',hashlib.sha256(target.read_bytes()).hexdigest());print('PASS dependency audit, clean-PATH primary/all enumeration, package file hashes and ZIP integrity')

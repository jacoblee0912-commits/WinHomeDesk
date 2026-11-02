from pathlib import Path
import argparse,zipfile,hashlib,json
p=argparse.ArgumentParser();p.add_argument('--base',required=True,type=Path);p.add_argument('--output',required=True,type=Path);a=p.parse_args()
w=Path(__file__).resolve().parent;repo=w.parent;out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
contents={}
with zipfile.ZipFile(a.base) as z:
 for name in z.namelist():
  parts=name.split('/',1)
  if len(parts)!=2 or name.endswith('/'):continue
  rel=parts[1]
  if rel.startswith(('engine/','licenses/')) or rel=='WinHomeDesk.ico':contents[rel]=z.read(name)
 contents['licenses/NOTICE.txt']=(w/'licenses/NOTICE.txt').read_bytes()
 old=json.loads(z.read('WinHomeDesk-0.2.9/SHA256.json'))
 for name,data in contents.items():
  if name.startswith('engine/'):assert hashlib.sha256(data).hexdigest()==old[name]
contents['WinHomeDesk.exe']=(w/'WinHomeDesk.exe').read_bytes()
for name in ['GUIDE.ko.md','GUIDE.en.md']:contents['docs/'+name]=(repo/'docs'/name).read_bytes()
for name in ['README.md','README.en.md']:contents[name]=(repo/name).read_bytes()
validation='''WinHomeDesk 0.3.0 / Experimental
Korean/English UI, catalog coverage, preference persistence, credential/address/group preservation,
live engine preservation, performance caption relocalization and GUI snapshots verified.
Native server engine and Korean input module unchanged from 0.2.9.
Actual Task Scheduler registration/UAC/re-login and Surface-to-Surface network operation were not retested.
\n'''
for name in ['localization030-tests.txt','startup029-tests.txt']:validation+=(w/name).read_text(encoding='utf-8')+'\n'
contents['VALIDATION.txt']=validation.encode('utf-8')
contents['SHA256.json']=json.dumps({n:hashlib.sha256(d).hexdigest() for n,d in contents.items()},ensure_ascii=False,indent=2).encode('utf-8')
asset=out/'WinHomeDesk-0.3.0-Windows-x64.zip'
with zipfile.ZipFile(asset,'w',zipfile.ZIP_DEFLATED,6) as z:
 for name,data in contents.items():z.writestr('WinHomeDesk-0.3.0/'+name,data)
with zipfile.ZipFile(asset) as z:assert z.testzip() is None
(out/'SHA256SUMS.txt').write_text(hashlib.sha256(asset.read_bytes()).hexdigest()+'  '+asset.name+'\n',encoding='utf-8')
print(asset,asset.stat().st_size)

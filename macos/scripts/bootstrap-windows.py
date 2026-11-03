#!/usr/bin/env python3
"""Project-local macOS -> Windows toolchain; official archives, SHA256 checked."""
import hashlib,io,json,pathlib,subprocess,tarfile,urllib.request,urllib.parse
import zstandard
ROOT=pathlib.Path(__file__).resolve().parents[2]
downloads=ROOT/'work/downloads';downloads.mkdir(exist_ok=True)
name='llvm-mingw-20261020-ucrt-macos-universal.tar.xz'
digest='e9c13e4ede38e715003ffc674d4d00a1088df8235439d1f0c16188ff0704c17e'
archive=downloads/name
if not archive.exists():urllib.request.urlretrieve('https://github.com/mstorsjo/llvm-mingw/releases/download/20261020/'+name,archive)
assert hashlib.sha256(archive.read_bytes()).hexdigest()==digest
if not (ROOT/'work/tooling'/name.removesuffix('.tar.xz')).exists():
    subprocess.run(['tar','-xf',str(archive),'-C',str(ROOT/'work/tooling')],check=True)
url='https://repo.msys2.org/mingw/ucrt64/'
db=downloads/'ucrt64.db';urllib.request.urlretrieve(url+'ucrt64.db',db)
def unpack_zstd(path):
    return tarfile.open(fileobj=io.BytesIO(zstandard.ZstdDecompressor().decompress(path.read_bytes(),max_output_size=300_000_000)))
with unpack_zstd(db) as tar:
    records=''.join(tar.extractfile(m).read().decode() for m in tar if m.name.endswith('/desc'))
wanted={'mingw-w64-ucrt-x86_64-'+x for x in ('openssl','zlib-ng-compat','openh264','libgcc','libstdc++','libwinpthread')}
packages=[]
for record in records.split('%FILENAME%\n')[1:]:
    lines=('%FILENAME%\n'+record).splitlines();fields={}
    for i,v in enumerate(lines):
        if v.startswith('%') and i+1<len(lines):fields[v]=lines[i+1]
    if fields.get('%NAME%') in wanted:packages.append(fields)
assert len(packages)==len(wanted),[p.get('%NAME%') for p in packages]
root=ROOT/'work/deps-windows';root.mkdir(exist_ok=True);manifest=[]
for package in packages:
    name=package['%FILENAME%'];archive=downloads/name;source=url+urllib.parse.quote(name)
    if not archive.exists():urllib.request.urlretrieve(source,archive)
    digest=hashlib.sha256(archive.read_bytes()).hexdigest();assert digest==package['%SHA256SUM%']
    with unpack_zstd(archive) as tar:
        for member in tar:
            if not member.isfile() or not member.name.startswith('ucrt64/'):continue
            target=root/member.name;target.resolve().relative_to(root.resolve())
            target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(tar.extractfile(member).read())
    manifest.append(dict(file=name,sha256=digest,source=source));print('Verified/extracted '+name,flush=True)
(root/'manifest.json').write_text(json.dumps(manifest,indent=2))

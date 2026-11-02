from pathlib import Path
import urllib.request,hashlib,sys,tarfile,io,json
sys.path.insert(0,'work/tooling');import zstandard
packages={
'openh264-2.7.0-1':'e73f2c6dc5afdb69d98da8ca89d494f606e5f5b01768071132dab8f02950cfae',
'libgcc-16.2.0-6':'80b2497f019977dd9528b1abc8621b1328a0115ab1f724047ccbb56bf323f11d',
'libstdc++-16.2.0-6':'33c5e58a85a538cf843a8672991cce591f0231d66da3b66a57629712d64be17f',
'libwinpthread-14.0.0.r569.g34eeafb7d-1':'b4ebe2826e848c5e790e06ea7b37d3e0594e005f42382b0506886f17df8127da'}
root=Path('work/deps')
for name,digest in packages.items():
 p=Path('work/downloads')/('mingw-w64-ucrt-x86_64-'+name+'-any.pkg.tar.zst')
 if not p.exists():urllib.request.urlretrieve('https://mirror.msys2.org/mingw/ucrt64/'+urllib.parse.quote(p.name),p)
 assert hashlib.sha256(p.read_bytes()).hexdigest()==digest,p
 data=zstandard.ZstdDecompressor().decompress(p.read_bytes(),max_output_size=30_000_000)
 with tarfile.open(fileobj=io.BytesIO(data)) as t:
  for m in t:
   if m.isfile() and m.name.startswith('ucrt64/'):
    dst=root/m.name;dst.resolve().relative_to(root.resolve());dst.parent.mkdir(parents=True,exist_ok=True);dst.write_bytes(t.extractfile(m).read())
 print(name,'verified/extracted')
Path('work/deps018-manifest.json').write_text(json.dumps(packages,indent=2))

from pathlib import Path
import subprocess,os,re,xml.etree.ElementTree as ET,json
w=Path(__file__).resolve().parent
entries=ET.parse(w/'Translations.xml').getroot().findall('text')
mapping={e.findtext('ko'):e.findtext('en') for e in entries}
assert len(mapping)==len(entries)
assert all(v and not re.search('[가-힣]',v) for v in mapping.values())
names=['SeparateMonitors.cs','MonitorSharing.cs','SurfaceDesk.cs','SurfaceDesk.Lifecycle.cs','DesktopOptions.cs','RdgManager.cs','RdcManClient.cs','PerformanceMetrics.cs','GraphicsMode.cs','EncoderMode.cs','AudioMode.cs','AppCredentials.cs','StartupRegistration.cs','Localization.cs']
for name in names:
 s=(w/name).read_text(encoding='utf-8-sig')
 for literal in re.findall(r'L\.T\(("(?:[^"\\]|\\.)*")\)',s):assert json.loads(literal) in mapping,(name,literal)
csc=Path(os.environ['WINDIR'])/'Microsoft.NET/Framework64/v4.0.30319/csc.exe'
subprocess.run([str(csc),'/nologo','/platform:x64','/out:'+str(w/'TestLocalization030.exe'),'/reference:'+str(w/'WinHomeDesk.exe'),'/reference:System.Windows.Forms.dll','/reference:System.Drawing.dll',str(w/'TestLocalization030.cs')],check=True)
p=subprocess.run([str(w/'TestLocalization030.exe'),str(w)],capture_output=True,timeout=45)
text=p.stdout.decode('utf-8',errors='replace')+p.stderr.decode('utf-8',errors='replace')
(w/'localization030-tests.txt').write_text('Catalog coverage PASS\n'+text,encoding='utf-8')
print(text);assert p.returncode==0

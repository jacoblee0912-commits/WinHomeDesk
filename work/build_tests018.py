from pathlib import Path
import subprocess,os
w=Path.cwd()/'work';csc=Path(os.environ['WINDIR'])/'Microsoft.NET/Framework64/v4.0.30319/csc.exe'
for name,refs in [('MicrosoftProbe',['System.dll','Microsoft.CSharp.dll']),('TestGraphics018',[str(w/'SurfaceDesk.exe'),'System.Security.dll']),('TestRdcSchema018',['System.Xml.dll'])]:
 subprocess.run([str(csc),'/nologo','/platform:x64','/out:'+str(w/(name+'.exe')),'/reference:System.Windows.Forms.dll','/reference:System.Drawing.dll',*['/reference:'+r for r in refs],str(w/(name+'.cs'))],check=True)
subprocess.run([str(w/'TestGraphics018.exe'),str(Path.cwd())],check=True)
for name in ['TestRdg','TestRdpImport']:
 subprocess.run([str(csc),'/nologo','/platform:x64','/out:'+str(w/(name+'.exe')),'/reference:System.Windows.Forms.dll','/reference:System.Drawing.dll','/reference:System.Xml.dll',str(w/(name+'.cs')),str(w/'RdgManager.cs')],check=True)
 subprocess.run([str(w/(name+'.exe')),str(Path.cwd())],check=True)
subprocess.run([str(w/'TestRdcSchema018.exe'),os.environ['RDCMAN_EXE'],str(w/'RDG-compatibility-test.rdg')],check=True)

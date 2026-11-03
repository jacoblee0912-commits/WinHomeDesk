#!/usr/bin/env python3
"""Create a relocatable, ad-hoc signed LOCAL app. No publishing or notarization."""
import os, pathlib, plistlib, shutil, subprocess
ROOT = pathlib.Path(__file__).resolve().parents[2]
BUILD = ROOT / 'work/build-mac'
APP = pathlib.Path(os.environ.get('WINHOMEDESK_PACKAGE_PATH',ROOT / 'output/macos/WinHomeDesk.app')).resolve()
APP.relative_to(ROOT/'output')
MAC = APP / 'Contents/MacOS'
LIB = APP / 'Contents/Frameworks'
RES = APP / 'Contents/Resources'
if LIB.exists(): shutil.rmtree(LIB)
for p in (MAC, LIB, RES): p.mkdir(parents=True, exist_ok=True)
shutil.copy2(ROOT/'output/macos/WinHomeDesk', MAC/'WinHomeDesk')
shutil.copy2(BUILD/'server/shadow/cli/freerdp-shadow-cli', MAC/'WinHomeDeskServer')
plist = dict(CFBundleIdentifier='com.winhomedesk.mac', CFBundleName='WinHomeDesk',
    CFBundleExecutable='WinHomeDesk', CFBundlePackageType='APPL', CFBundleVersion='0.7.0',
    CFBundleShortVersionString='0.7.0-local', LSMinimumSystemVersion='27.0', NSHighResolutionCapable=True,
    NSScreenCaptureUsageDescription='Share this Mac screen and optional system audio with an authenticated RDP client.',
    NSAccessibilityUsageDescription='Allow the authenticated RDP client to control keyboard and mouse.',
    NSLocalNetworkUsageDescription='Connect to Windows PCs and accept remote desktop connections on your LAN.')
with (APP/'Contents/Info.plist').open('wb') as f: plistlib.dump(plist,f)
queue = list(MAC.iterdir()); seen=set()
while queue:
    binary=queue.pop()
    if binary in seen: continue
    seen.add(binary)
    deps=subprocess.check_output(['otool','-L',str(binary)],text=True).splitlines()[1:]
    for dep in deps:
        name=dep.strip().split(' (')[0]
        if name.startswith(('/System/','/usr/lib/')): continue
        if name.startswith('@rpath/'):
            leaf=pathlib.Path(name).name
            target=LIB/leaf
            if not target.exists():
                candidates=list(BUILD.rglob(leaf))
                if not candidates: raise RuntimeError('Missing dependency '+name)
                shutil.copy2(candidates[0].resolve(),target); queue.append(target)
        elif name.startswith(str(ROOT)):
            target=LIB/pathlib.Path(name).name
            if not target.exists(): shutil.copy2(name,target); queue.append(target)
            subprocess.run(['install_name_tool','-change',name,'@rpath/'+target.name,str(binary)],check=True)
        else: raise RuntimeError('Unpackaged dependency '+name)
    if binary.parent==LIB: subprocess.run(['install_name_tool','-id','@rpath/'+binary.name,str(binary)],check=True)
    # Remove build-machine paths, then use only bundle-relative lookup.
    lines=subprocess.check_output(['otool','-l',str(binary)],text=True).splitlines()
    for n,line in enumerate(lines):
        if line.strip()=='cmd LC_RPATH':
            path=lines[n+2].strip().split('path ',1)[1].split(' (offset')[0]
            subprocess.run(['install_name_tool','-delete_rpath',path,str(binary)],check=True)
    subprocess.run(['install_name_tool','-add_rpath','@executable_path/../Frameworks',str(binary)],check=True)
for name, source in [('WinHomeDesk-LICENSE.txt',ROOT/'LICENSE'),('FreeRDP-LICENSE.txt',ROOT/'work/FreeRDP/LICENSE'),('OpenH264-LICENSE.txt',ROOT/'work/deps/openh264-2.6.0/LICENSE'),('OpenSSL-LICENSE.txt',ROOT/'work/deps/openssl-3.5.4/LICENSE.txt')]:
    shutil.copy2(source,RES/name)
for binary in seen: subprocess.run(['codesign','--force','--sign','-',str(binary)],check=True)
subprocess.run(['codesign','--force','--sign','-',str(APP)],check=True)
subprocess.run(['codesign','--verify','--deep','--strict',str(APP)],check=True)
print(APP)

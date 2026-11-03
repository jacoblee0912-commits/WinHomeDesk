#!/usr/bin/env python3
"""Package a local Windows engine update; never downloads GUI or publishes."""
import hashlib,json,pathlib,re,shutil,subprocess
ROOT=pathlib.Path(__file__).resolve().parents[2]
BUILD=ROOT/'work/build-windows';DEP=ROOT/'work/deps-windows/ucrt64'
LLVM=ROOT/'work/tooling/llvm-mingw-20261020-ucrt-macos-universal'
OUT=ROOT/'output/windows-multimonitor';ENGINE=OUT/'engine'
ENGINE.mkdir(parents=True,exist_ok=True)
system=set('advapi32 avrt bcrypt cabinet comctl32 comdlg32 credui crypt32 d3d11 d3dcompiler_47 dbghelp dnsapi dwmapi dxgi gdi32 imagehlp imm32 iphlpapi kernel32 mf mfplat mfreadwrite mpr msvcrt ncrypt netapi32 ntdll ole32 oleaut32 powrprof propsys psapi rpcrt4 secur32 setupapi shell32 shlwapi user32 userenv version winhttp wininet winmm winspool winscard ws2_32 wtsapi32'.split())
index={p.name.lower():p for folder in (DEP/'bin',LLVM/'x86_64-w64-mingw32/bin',BUILD) for p in folder.rglob('*.dll')}
queue=[(BUILD/'server/shadow/cli/freerdp-shadow-cli.exe',ENGINE/'WinHomeDesk-engine.exe'),(DEP/'lib/ossl-modules/legacy.dll',ENGINE/'ossl-modules/legacy.dll')]
done=set();audit={}
while queue:
    source,target=queue.pop()
    if target in done:continue
    target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source,target);done.add(target)
    info=subprocess.check_output([str(LLVM/'bin/llvm-readobj'),'--coff-imports',str(target)],text=True)
    assert 'Format: COFF-x86-64' in info,target
    dependencies=re.findall(r'^  Name: (.+\.dll)$',info,re.M|re.I);audit[str(target.relative_to(OUT))]=dependencies
    for name in dependencies:
        lower=name.lower()
        if lower in index:queue.append((index[lower],ENGINE/name))
        elif lower.startswith(('api-ms-win-','ext-ms-win-')) or lower.removesuffix('.dll') in system:continue
        else:raise RuntimeError('Unpackaged dependency '+name)
(ENGINE/'WinHomeDesk-all-monitors.flag').write_text('WinHomeDesk multi-monitor opt-in; remove while server is stopped to restore primary-only capture.\n')
manifest=[dict(path=str(p.relative_to(ENGINE)).replace('/','\\'),sha256=hashlib.sha256(p.read_bytes()).hexdigest()) for p in sorted(ENGINE.rglob('*')) if p.is_file()]
(OUT/'engine-sha256.json').write_text(json.dumps(manifest,indent=2))
(OUT/'dependency-audit.json').write_text(json.dumps(audit,indent=2))
shutil.copy2(ROOT/'macos/scripts/Update-WindowsMonitors.ps1',OUT/'Update-WindowsMonitors.ps1')
shutil.copy2(ROOT/'macos/windows-monitor-update.md',OUT/'README-ko.txt')
(OUT/'Update.cmd').write_bytes(b'@echo off\r\npowershell.exe -NoProfile -STA -ExecutionPolicy Bypass -File "%~dp0Update-WindowsMonitors.ps1" %*\r\n')
shutil.copytree(DEP/'share/licenses',OUT/'licenses',dirs_exist_ok=True)
for name,source in [('WinHomeDesk.txt',ROOT/'LICENSE'),('FreeRDP.txt',ROOT/'work/FreeRDP/LICENSE'),('MinGW-runtime.txt',LLVM/'x86_64-w64-mingw32/share/mingw32/COPYING.MinGW-w64-runtime.txt')]:shutil.copy2(source,OUT/'licenses'/name)
shutil.copy2(ROOT/'work/deps-windows/manifest.json',OUT/'licenses/dependencies.json')
shutil.make_archive(str(ROOT/'output/WinHomeDesk-Windows-MultiMonitor-0.7.0-local'),'zip',OUT)
print(f'Packaged {len(manifest)} files; all PE imports accounted for. '+str(OUT))

#!/usr/bin/env python3
"""Prepare local Windows test files; never copies into synced folders or sends them."""
import argparse
import ipaddress
import pathlib
import shutil
import xml.etree.ElementTree as ET
import zipfile

ROOT = pathlib.Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser()
p.add_argument('--mac-address', required=True, type=ipaddress.IPv4Address)
p.add_argument('--port', type=int, default=3390)
p.add_argument('--rdcman293', required=True)
p.add_argument('--rdcman322', required=True)
p.add_argument('--output', type=pathlib.Path, default=ROOT/'output/windows-validation')
a = p.parse_args()
if not 1 <= a.port <= 65535:
    p.error('Port must be 1–65535')
a.output.mkdir(parents=True, exist_ok=True)


def ps(value):
    return "'" + str(value).replace("'", "''") + "'"


def write(name, text, encoding='utf-8-sig'):
    (a.output/name).write_text(text.replace('\r\n', '\n').replace('\n', '\r\n'), encoding=encoding)


for version in ('2.93', '3.22'):
    root = ET.Element('RDCMan', programVersion=version, schemaVersion='3')
    f = ET.SubElement(root, 'file')
    ET.SubElement(f, 'credentialsProfiles')
    props = ET.SubElement(f, 'properties')
    ET.SubElement(props, 'expanded').text = 'True'
    ET.SubElement(props, 'name').text = 'WinHomeDesk Mac validation'
    server = ET.SubElement(f, 'server')
    props = ET.SubElement(server, 'properties')
    ET.SubElement(props, 'displayName').text = 'Mac - ' + version
    ET.SubElement(props, 'name').text = str(a.mac_address)
    logon = ET.SubElement(server, 'logonCredentials', inherit='None')
    ET.SubElement(logon, 'profileName', scope='Local').text = 'Custom'
    ET.SubElement(logon, 'userName').text = 'surface'
    ET.SubElement(logon, 'password')
    ET.SubElement(logon, 'domain')
    connection = ET.SubElement(server, 'connectionSettings', inherit='None')
    ET.SubElement(connection, 'connectToConsole').text = 'False'
    ET.SubElement(connection, 'port').text = str(a.port)
    security = ET.SubElement(server, 'securitySettings', inherit='None')
    ET.SubElement(security, 'authentication').text = 'Warn'
    desktop = ET.SubElement(server, 'remoteDesktop', inherit='None')
    ET.SubElement(desktop, 'colorDepth').text = '32'
    for node in ('connected', 'favorites', 'recentlyUsed'):
        ET.SubElement(root, node)
    ET.indent(root)
    ET.ElementTree(root).write(a.output/f'Mac-{version}.rdg', encoding='utf-8', xml_declaration=True)

# BOM is required for Korean paths in Windows PowerShell 5.1 scripts.
write('Test-RdcMan.ps1', (ROOT/'macos/tests/rdcman.ps1').read_text())
write('Run-Validation.ps1', f"""$ErrorActionPreference='Stop'
& (Join-Path $PSScriptRoot 'Test-RdcMan.ps1') -MacAddress {ps(a.mac_address)} -Port {a.port} `
  -RdcMan293 {ps(a.rdcman293)} -RdcMan322 {ps(a.rdcman322)} `
  -OutputDirectory (Join-Path $PSScriptRoot 'results')
""")
write('Check-Windows.ps1', f"""$ErrorActionPreference='Continue'
Write-Host 'Windows hostname and user (no password):'
hostname
whoami
Write-Host 'RDP listening ports:'
Get-NetTCPConnection -State Listen | Where-Object {{ $_.LocalPort -in 3389,3390 }} | Select-Object LocalAddress,LocalPort,OwningProcess
Write-Host 'Mac reachability:'
Test-NetConnection -ComputerName {ps(a.mac_address)} -Port {a.port}
Write-Host 'RDCMan file versions:'
foreach($file in @({ps(a.rdcman293)},{ps(a.rdcman322)})) {{
 if(Test-Path -LiteralPath $file) {{ (Get-Item -LiteralPath $file).VersionInfo | Select-Object FileName,FileVersion,ProductVersion }}
 else {{ Write-Warning ('File not found: '+$file) }}
}}
""")
for name, script in [('01-Check.cmd','Check-Windows.ps1'), ('02-Test-RDCMan.cmd','Run-Validation.ps1')]:
    write(name, f'@echo off\npowershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0{script}"\npause\n', 'ascii')
write('사용안내.txt', f"""WinHomeDesk Windows ↔ Mac 실기 검증

1. Mac에서 WinHomeDesk.app을 실행합니다.
   Mac 수신 IP: {a.mac_address}, Mac RDP 포트: {a.port}
   8자 이상 앱 암호를 입력하고 화면 공유 동의 후 서버를 시작합니다.
   화면 기록 및 손쉬운 사용 권한이 필요합니다.
2. 이 ZIP을 Windows의 원하는 폴더에 풉니다.
3. 01-Check.cmd를 실행해 연결과 RDCMan 실행 파일 버전을 확인합니다.
   False/시간 초과이면 화면·입력 시험을 통과한 것이 아닙니다.
4. 02-Test-RDCMan.cmd를 실행합니다. 두 버전을 차례로 엽니다.
   서버 목록의 Mac 항목에 접속하고, 암호는 RDCMan 창에 직접 입력합니다.
   사용자: surface / Domain: 빈칸
   직접 여는 방법: 해당 RDCMan 버전의 File > Open에서 Mac-2.93.rdg 또는 Mac-3.22.rdg 선택.
5. 실제 화면·입력 동작을 확인한 항목만 pass, 실패는 fail, 미시험은 skip으로 입력합니다.
   먼저 Mac의 빈 TextEdit 문서를 준비해 시험하세요.
   두 버전을 동시에 연결하지 마세요. 현재 Mac 서버는 제어 연결 한 개만 허용합니다.
6. 결과는 이 폴더의 results/results.json입니다. 자동 업로드하지 않습니다.

버전별 실행 파일 경로:
2.93: {a.rdcman293}
3.22: {a.rdcman322}

암호·RDCMan 실행 파일은 패키지에 포함하지 않습니다.
기존 Windows 서버 설정·방화벽·원격 데스크톱 활성화 상태는 변경하지 않습니다.
이 패키지의 Windows 실행 및 RDG 호환성은 실제 시험 전에는 미검증입니다.
""")
archive = a.output.with_suffix('.zip')
with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED) as z:
    for name in ('Mac-2.93.rdg','Mac-3.22.rdg','Test-RdcMan.ps1','Run-Validation.ps1','Check-Windows.ps1','01-Check.cmd','02-Test-RDCMan.cmd','사용안내.txt'):
        z.write(a.output/name, 'WinHomeDesk-Windows-Test/'+name)
print(archive)

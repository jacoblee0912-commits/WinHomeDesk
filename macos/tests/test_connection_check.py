#!/usr/bin/env python3
"""Probe test peer, no credentials and no access to a real desktop."""
from pathlib import Path
import socket
import subprocess
import threading
import json
import os
ROOT=Path(__file__).resolve().parents[2]
APP=Path(os.environ.get('WINHOMEDESK_TEST_APP',ROOT/'output/macos/WinHomeDesk.app'))/'Contents/MacOS/WinHomeDesk'
request=bytes.fromhex('030000130ee000000000000100080003000000')
results=[]
def run_case(name, response, expected):
    errors=[]
    with socket.socket() as listener:
        listener.bind(('127.0.0.1',0)); listener.listen(1); listener.settimeout(6)
        port=listener.getsockname()[1]
        def peer():
            try:
                with listener.accept()[0] as client:
                    client.settimeout(5); got=b''
                    while len(got)<len(request):
                        chunk=client.recv(len(request)-len(got))
                        if not chunk: break
                        got+=chunk
                    assert got==request, 'Probe must contain only the X.224 negotiation request'
                    # Fragmented TCP responses must be reassembled.
                    for byte in response: client.sendall(bytes([byte]))
            except Exception as e: errors.append(str(e))
        thread=threading.Thread(target=peer);thread.start()
        result=subprocess.run([str(APP),'--diagnose','127.0.0.1',str(port)],capture_output=True,text=True,timeout=7)
        thread.join(timeout=6)
        passed=result.returncode==expected and not errors and not thread.is_alive()
        results.append(dict(test=name,passed=passed,output=result.stdout.strip(),errors=errors))
run_case('NLA',bytes.fromhex('030000130ed000000000000200080002000000'),0)
run_case('TLS',bytes.fromhex('030000130ed000000000000200080001000000'),0)
run_case('invalid_tpkt',bytes.fromhex('04000004'),1)
run_case('short_packet',bytes.fromhex('03000013'),1)
run_case('invalid_protocol',bytes.fromhex('030000130ed0000000000002000800ffffffff'),1)
for item in results: print(json.dumps(item))
(ROOT/'output/macos/connection-check-results.json').write_text(json.dumps(results,indent=2))
raise SystemExit(0 if all(row['passed'] for row in results) else 1)

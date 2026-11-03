"""Native cursor-flicker regression: no real mouse movement or display changes.
Settings HRESULT regression is included in test029.py.
"""
from pathlib import Path
import argparse, subprocess
p=argparse.ArgumentParser();p.add_argument('--compiler',required=True);a=p.parse_args()
w=Path(__file__).resolve().parent
subprocess.run([a.compiler,str(w/'test_display033.c'),'-o',str(w/'test_display033.exe'),'-luser32'],check=True)
subprocess.run([str(w/'test_display033.exe')],check=True,timeout=20)

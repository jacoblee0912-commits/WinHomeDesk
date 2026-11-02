from pathlib import Path
import subprocess,os,shutil
w=Path.cwd()/'work';root=w/'FreeRDP'
p=root/'CMakeLists.txt';s=p.read_text()
if 'surface-pipeline-probe' not in s:
 s+='\nadd_executable(surface-pipeline-probe ../test_pipeline020.c)\nadd_executable(surface-benchmark ../benchmark020.c)\ntarget_link_libraries(surface-benchmark PRIVATE freerdp winpr)\n'
 p.write_text(s)
env=os.environ.copy();env['PATH']=str(w/'compiler/llvm-mingw-20261020-ucrt-x86_64/bin')+';'+env['PATH']
with (w/'build020.log').open('w') as log:
 p=subprocess.run([str(w/'tooling/cmake/data/bin/cmake.exe'),'--build',str(w/'build-ready'),'--target','freerdp-shadow-cli','surface-probe','surface-codec-probe','surface-pipeline-probe','surface-benchmark','--parallel','6'],env=env,stdout=log,stderr=subprocess.STDOUT)
print((w/'build020.log').read_text(errors='replace')[-2200:]);assert p.returncode==0
for p in (w/'build-ready').rglob('*.dll'):shutil.copy2(p,w/'runtime'/p.name)
for name in ['surface-benchmark','surface-codec-probe','surface-probe']:
 shutil.copy2(w/'build-ready'/(name+'.exe'),w/'runtime'/(name+'.exe'))
shutil.copy2(w/'build-ready/server/shadow/cli/freerdp-shadow-cli.exe',w/'runtime/surface-rdp-engine.exe')

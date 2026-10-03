#!/usr/bin/env python3
import hashlib,json,os,stat,subprocess,sys
from pathlib import Path
prep=Path(__file__).resolve().parent
root=prep.parents[2]
mode=sys.argv[1]
c=json.loads((prep/(mode+'-build-request.json')).read_text())
pin=Path(c['pin'])
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def verify_source():
 for name,want in c['source_sha256'].items():
  if sha(root/name)!=want:raise SystemExit('Source changed: '+name)
verify_source()
if pin.exists():raise SystemExit('Fresh build directory already exists: '+str(pin))
commands=[['cmake','-S',str(root),'-B',str(pin),'-DCMAKE_BUILD_TYPE='+c['build_type'],'-DCMAKE_C_COMPILER='+c['cc'],'-DCMAKE_CXX_COMPILER='+c['cxx'],'-DTACTIX_ENABLE_PROOF_SHADOW='+('ON' if c['proof_shadow'] else 'OFF'),'-DTACTIX_ENABLE_EVENT_TRACE=OFF','-DTACTIX_ENABLE_RULE_JOURNAL=OFF'],['cmake','--build',str(pin),'-j',str(c['build_parallelism'])]]
if mode=='release':commands[-1]+= ['--target','tactix']
for i,command in enumerate(commands):
 with (prep/(mode+'-'+('configure' if i==0 else 'build')+'.log')).open('w') as log:
  subprocess.run(command,cwd=root,stdout=log,stderr=subprocess.STDOUT,check=True)
verify_source()
metadata={'schema':'tactix-controller-binary-pin-v1','source_commit':c['source_commit'],'build_type':c['build_type'],'proof_shadow':c['proof_shadow'],'source_sha256':c['source_sha256'],'binaries':{name:sha(pin/name) for name in c['expected_binaries']}}
(pin/'PIN.json').write_text(json.dumps(metadata,indent=2)+'\n')
# Keep CTest scratch writable, freeze binaries/configuration and all other assets.
for base,dirs,files in os.walk(pin):
 for name in files:
  p=Path(base)/name
  if not p.is_symlink():p.chmod(stat.S_IMODE(p.stat().st_mode)&~0o222)
result={'mode':mode,'source_commit':c['source_commit'],'pin':str(pin),'tactix_sha256':metadata['binaries']['tactix'],'source_bindings':len(metadata['source_sha256']),'executables':len(metadata['binaries']),'build_exit':0,'configuration':commands[0],'build':commands[1],'pin_metadata_sha256':sha(pin/'PIN.json')}
(prep/(mode+'-build-result.json')).write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result,indent=2),flush=True)

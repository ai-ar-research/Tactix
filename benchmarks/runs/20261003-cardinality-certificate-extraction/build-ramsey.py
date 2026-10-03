import hashlib,json,shlex,subprocess,shutil
from pathlib import Path
root=Path(__file__).resolve().parents[3]
base=root/'build/pin-parity-public-release-6fe14539-01'
pin=root/'build/pin-cardinality-shared-47726643-01'
assert not (pin/'manifest.json').exists()
pin.mkdir(exist_ok=True)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
request=json.loads((root/'benchmarks/runs/20261003-parity-public-action/release-build-request.json').read_text())
print('Request keys:',list(request),flush=True)
# Use the same source binding authority as the candidate pin.
meta=json.loads((base/'PIN.json').read_text())
print('PIN keys:',list(meta),flush=True)
source_hashes=meta['source_sha256']
source_hashes['src/proof/cardinality_certificate.hpp']=sha(root/'src/proof/cardinality_certificate.hpp')
for name,digest in source_hashes.items():assert sha(root/name)==digest,name
for name in ('certificate.cpp','recognize.hpp'):
 shutil.copyfile(root/'benchmarks/probes/scratch/ramsey-recognition'/('certificate-shared.cpp' if name=='certificate.cpp' else name),pin/name)
cmd=['/cm/local/apps/gcc/13.1.0/bin/c++','-O3','-DNDEBUG','-std=c++20','-I'+str(root/'src'),'-I'+str(root/'include'),'-c',str(pin/'certificate.cpp'),'-o',str(pin/'certificate.o')]
subprocess.run(cmd,check=True)
raw=subprocess.check_output(['ninja','-C',str(base),'-t','commands','tactix'],text=True).splitlines()[-1]
assert raw.startswith(': && ') and raw.endswith(' && :')
link=shlex.split(raw[5:-5])
removed=[]
for suffix in ('/src/main.cpp.o','/src/rules/lucky/signed_cardinality_proof.cpp.o'):
 selected=[v for v in link if v.endswith(suffix)];assert len(selected)==1
 link.remove(selected[0]);removed+=selected
link.append(str(pin/'certificate.o'))
link[link.index('-o')+1]=str(pin/'probe')
inputs={str((base/v).relative_to(root)):sha(base/v) for v in link if v.endswith(('.o','.a')) and not Path(v).is_absolute()}
subprocess.run(link,cwd=base,check=True)
for name,digest in source_hashes.items():assert sha(root/name)==digest,name
for name,digest in inputs.items():assert sha(root/name)==digest,name
bindings={str(p.relative_to(root)):sha(p) for p in [Path(__file__),pin/'certificate.cpp',pin/'recognize.hpp',pin/'certificate.o',pin/'probe',base/'PIN.json']}
(pin/'manifest.json').write_text(json.dumps({'base':'477266433 plus new shared header and current parity/graph worktree; isolated differential only','compile':cmd,'link':link,'link_cwd':str(base),'removed':removed,'source_hashes':source_hashes,'link_inputs':inputs,'bindings':bindings},indent=2)+'\n')
print('Built',pin,flush=True)

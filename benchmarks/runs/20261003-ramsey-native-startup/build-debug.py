import hashlib,json,shlex,subprocess,shutil
from pathlib import Path
root=Path(__file__).resolve().parents[3]
base=root/'build/pin-parity-public-debug-6fe14539-01'
pin=root/'build/pin-ramsey-native-e5f5d656-debug-01'
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
source_hashes['src/proof/ramsey_certificate.hpp']=sha(root/'src/proof/ramsey_certificate.hpp')
for name,digest in source_hashes.items():assert sha(root/name)==digest,name
for name in ('src/rules/lucky/ramsey_recognition.hpp','src/rules/lucky/ramsey_startup.hpp','src/rules/lucky/ramsey_startup.cpp','tests/test_ramsey_startup.cpp'):
 source_hashes[name]=sha(root/name)
 shutil.copyfile(root/name,pin/Path(name).name)
shutil.copyfile(root/'tests/test_ramsey_startup.cpp',pin/'certificate.cpp')
cmd=['/cm/local/apps/gcc/13.1.0/bin/c++','-O0','-g','-DTACTIX_ENABLE_PROOF_SHADOW','-std=c++20','-I'+str(root/'src'),'-I'+str(root/'include'),'-c',str(pin/'certificate.cpp'),'-o',str(pin/'certificate.o')]
subprocess.run(cmd,check=True)
native=list(cmd);native[native.index('-c')+1]=str(pin/'ramsey_startup.cpp');native[native.index('-o')+1]=str(pin/'ramsey_startup.o');subprocess.run(native,check=True)
raw=subprocess.check_output(['ninja','-C',str(base),'-t','commands','tactix'],text=True).splitlines()[-1]
assert raw.startswith(': && ') and raw.endswith(' && :')
link=shlex.split(raw[5:-5])
removed=[]
for suffix in ('/src/main.cpp.o',):
 selected=[v for v in link if v.endswith(suffix)];assert len(selected)==1
 link.remove(selected[0]);removed+=selected
link.extend([str(pin/'certificate.o'),str(pin/'ramsey_startup.o')])
link[link.index('-o')+1]=str(pin/'probe')
inputs={str((base/v).relative_to(root)):sha(base/v) for v in link if v.endswith(('.o','.a')) and not Path(v).is_absolute()}
subprocess.run(link,cwd=base,check=True)
for name,digest in source_hashes.items():assert sha(root/name)==digest,name
for name,digest in inputs.items():assert sha(root/name)==digest,name
bindings={str(p.relative_to(root)):sha(p) for p in [Path(__file__),pin/'certificate.cpp',pin/'ramsey_startup.cpp',pin/'ramsey_recognition.hpp',pin/'ramsey_startup.hpp',pin/'ramsey_startup.o',pin/'certificate.o',pin/'probe',base/'PIN.json']}
(pin/'manifest.json').write_text(json.dumps({'base':'e5f5d656 plus internal native Ramsey adapter and current parity/graph worktree; no public action acceptance','compile':cmd,'link':link,'link_cwd':str(base),'removed':removed,'source_hashes':source_hashes,'link_inputs':inputs,'bindings':bindings},indent=2)+'\n')
print('Built',pin,flush=True)

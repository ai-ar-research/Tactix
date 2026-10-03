import json,subprocess,hashlib
from pathlib import Path
root=Path(__file__).resolve().parents[3];pin=root/'build/pin-ramsey-r34-dsr-bcf71ced-01'
assert not (pin/'manifest.json').exists()
pin.mkdir(exist_ok=True)
source=root/'benchmarks/probes/scratch/ramsey_r34_dsr_probe.cpp'
(pin/'probe.cpp').write_bytes(source.read_bytes())
compiler='/cm/local/apps/gcc/13.1.0/bin/c++';obj=pin/'probe.o'
cmd=[compiler,'-O3','-DNDEBUG','-std=c++20','-I'+str(root/'src'),'-I'+str(root/'include'),'-c',str(pin/'probe.cpp'),'-o',str(obj)]
subprocess.run(cmd,check=True)
parent=root/'build/pin-winner-graph-reflection-113b3472-01/provenance.json';m=json.loads(parent.read_text())
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
assert all(sha(root/p)==h for p,h in m['source_hashes'].items())
assert all(sha(root/p)==h for p,h in m['link_inputs'].items())
link=m['link_command'];old=str(root/'build/pin-winner-graph-probe-d85b30b4-01/graph-profile-main.cpp.o')
assert link.count(old)==1;link[link.index(old)]=str(obj)
removed=[s for s in link if s.endswith('/signed_cardinality_proof.cpp.o')]
assert len(removed)==1;link.remove(removed[0]);link[link.index('-o')+1]=str(pin/'probe')
subprocess.run(link,check=True)
(pin/'manifest.json').write_text(json.dumps(dict(compile_command=cmd,link_command=link,removed_original_object=removed[0],parent=str(parent.relative_to(root)),scope='Isolated DSR feasibility for complete R(3,4) clause set, not a public rule or target result.',bindings={str(p.relative_to(root)):sha(p) for p in [source,Path(__file__),pin/'probe.cpp',obj,pin/'probe',parent,root/'src/rules/lucky/signed_cardinality_proof.cpp']}),indent=2)+'\n')
print('Built',pin)

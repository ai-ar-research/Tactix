import hashlib,json,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[3];run=Path(__file__).resolve().parent
pin=root/'build/pin-parity-congruence-overlap-bb75b98f-01';assert not (run/'manifest.json').exists();pin.mkdir(exist_ok=True)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
parent=root/'build/pin-winner-graph-reflection-113b3472-01/provenance.json';m=json.loads(parent.read_text())
assert all(sha(root/p)==h for p,h in m['source_hashes'].items())
assert all(sha(root/p)==h for p,h in m['link_inputs'].items())
source=run/'probe.cpp';obj=pin/'probe.o';cmd=['/cm/local/apps/gcc/13.1.0/bin/c++','-O2','-std=c++20','-I'+str(root/'src'),'-I'+str(root/'include'),'-c',str(source),'-o',str(obj)]
subprocess.run(cmd,check=True)
link=m['link_command'];old=str(root/'build/pin-winner-graph-probe-d85b30b4-01/graph-profile-main.cpp.o');assert link.count(old)==1
link[link.index(old)]=str(obj);link[link.index('-o')+1]=str(pin/'probe');subprocess.run(link,check=True)
p=subprocess.run([str(pin/'probe')],capture_output=True,text=True,timeout=30)
(run/'stdout.jsonl').write_text(p.stdout);(run/'stderr.txt').write_text(p.stderr);assert p.returncode==0,p.stderr
records=[json.loads(s) for s in p.stdout.splitlines()];assert len(records)==2
(run/'result.json').write_text(json.dumps({'scope':'Fixed native Congruence microcontrols, not a benchmark or new implementation','returncode':p.returncode,'cases':records},indent=2)+'\n')
(run/'manifest.json').write_text(json.dumps({'parent':str(parent.relative_to(root)),'compile':cmd,'link':link,'bindings':{str(p.relative_to(root)):sha(p) for p in [parent,source,Path(__file__),obj,pin/'probe',root/'src/rules/congruence/congruence.cpp',run/'result.json']}},indent=2)+'\n')
print(p.stdout)

from pathlib import Path
import hashlib,json,subprocess
root=Path(__file__).resolve().parents[3];run=Path(__file__).resolve().parent
pin=root/'build/pin-parity-live-adapter-2d68c45f-04';pin.mkdir(exist_ok=True)
assert not (pin/'manifest.json').exists()
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
parent=root/'build/pin-winner-graph-reflection-113b3472-01/provenance.json';m=json.loads(parent.read_text())
assert all(sha(root/p)==h for p,h in m['source_hashes'].items() if p not in ['src/rules/lucky/parity_refutation.cpp','src/kernel/kernel.cpp','src/kernel/kernel.hpp'])
assert all(sha(root/p)==h for p,h in m['link_inputs'].items())
compiler='/cm/local/apps/gcc/13.1.0/bin/c++'
flags=[compiler,'-O2','-std=c++20','-Wall','-Wextra','-I'+str(root/'src'),'-I'+str(root/'include')]
obj=pin/'kernel.o';subprocess.run(flags+['-DNDEBUG','-c',str(root/'src/kernel/kernel.cpp'),'-o',str(obj)],check=True)
liveobj=pin/'parity_live.o';subprocess.run(flags+['-DNDEBUG','-c',str(root/'src/rules/lucky/parity_live.cpp'),'-o',str(liveobj)],check=True)
records=[]
for name in ['test_parity_live']:
 source=root/'tests'/f'{name}.cpp';testobj=pin/(name+'.o');subprocess.run(flags+['-c',str(source),'-o',str(testobj)],check=True)
 link=m['link_command'].copy();old=str(root/'build/pin-winner-graph-probe-d85b30b4-01/graph-profile-main.cpp.o');link[link.index(old)]=str(testobj)
 oldobj=next(s for s in link if s.endswith('/kernel.cpp.o'));link[link.index(oldobj)]=str(obj);link[link.index('-o')+1]=str(pin/name)
 link.insert(3,str(liveobj));subprocess.run(link,check=True);p=subprocess.run([str(pin/name),str(run/'nonterminal.dsr'),str(run/'provenance.jsonl')],capture_output=True,text=True,timeout=60)
 (run/(name+'.stdout')).write_text(p.stdout);(run/(name+'.stderr')).write_text(p.stderr);assert p.returncode==0,p.stderr
 records.append({'test':name,'returncode':p.returncode,'binary_sha256':sha(pin/name),'link':link});print(name,'passed',flush=True)
paths=[parent,root/'src/kernel/kernel.cpp',root/'src/kernel/kernel.hpp',root/'tests/test_parity_live.cpp',root/'src/rules/lucky/parity_live.cpp',root/'src/rules/lucky/parity_live.hpp',root/'src/rules/lucky/parity_certificate.hpp',root/'src/rules/lucky/parity_premises.hpp',root/'src/rules/lucky/parity_selection.hpp',Path(__file__),obj]
(pin/'manifest.json').write_text(json.dumps({'parent':str(parent.relative_to(root)),'results':records,'bindings':{str(p.relative_to(root)):sha(p) for p in paths}},indent=2)+'\n')
(run/'native-result.json').write_bytes((pin/'manifest.json').read_bytes())

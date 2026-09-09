#!/usr/bin/env python3
"""After the complete B/C solver gate, replay only repaired Rank45."""
import json
from pathlib import Path
import shutil
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
gate=root/'benchmarks/runs/20260909-6427-activation-stamp-BC-gate/solver-checkpoint-6429.json'
if not gate.exists(): print('Complete6429 B/C solver gate required; no output created'); sys.exit(0)
g=json.loads(gate.read_text()); assert g['solver_records']==66
authority=root/'benchmarks/runs/20260909-6424-activation-stamp-build/manifest.json'
b=json.loads(authority.read_text())
for rel,digest in b['files'].items(): assert sha(root/b['source']/rel)==digest
old=root/'benchmarks/runs/20260909-6402-rank45-stamp-prefix'; m=json.loads((old/'manifest.json').read_text())
for rel,digest in m['hashes'].items(): assert sha(old/rel)==digest
control=root/'benchmarks/runs/20260909-6415-rank45-transitive-schedule/summary.json'
s=json.loads(control.read_text()); assert s['parent_replay']
for call in s['comparisons']:
    diff=call['pairs']['candidate_kissat']['input_differences']; assert len(diff)==1
    assert diff[0]['left']['stamp']==5323 and diff[0]['right']['stamp']==1
c=root/'benchmarks/runs/20260909-6433-activation-rank45-prefix'; assert not c.exists()
for name in ('pins','logs','results','traces'): (c/name).mkdir(parents=True)
binary=root/'build/pin-6424-activation-stamp-trace/tactix'
assert sha(binary)==b['binaries'][str(binary.relative_to(root))]
shutil.copy2(binary,c/'pins/candidate')
runner=(old/'run.py').read_text().replace("('candidate', 'previous', 'kissat')","('candidate',)").replace('tactix-6402-','tactix-6433-')
compile(runner,str(c/'run.py'),'exec'); (c/'run.py').write_text(runner)
(c/'array.sbatch').write_text((old/'array.sbatch').read_text().replace(str(old),str(c)).replace('--array=0-2','--array=0').replace('6402-rank45-stamp-prefix','6433-activation-rank45'))
binding=lambda p:dict(path=str(p.relative_to(root)),sha256=sha(p))
m.update(authorities=dict(candidate=binding(authority)),parent_summary=binding(old/'summary.json'),
    gate=binding(gate),schedule_control=binding(control),cached_native=binding(old/'results/kissat.json'),
    purpose='Test whether native activation stamp1 closes prior Rank45 divergence83117 and restores second76464 reductions. Candidate-only against validated cached native; no new timing/reference run.',
    validation='Check complete100K stream accounting/cleanup and compare every row with cached6402 native. Record first difference or full closure, never inherit equality from scalar counts.',
    test_consideration=b['test_consideration'],limitation='Changed scheduling work is allowed. No exact predecessor proof claim, final-proof acceptance or full Rank32/50 closure.')
m['resource_contract']['array']='0'
m['hashes']={rel:sha(c/rel) for rel in ('pins/candidate','run.py','array.sbatch')}
(c/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
(c/'pins/candidate').chmod((c/'pins/candidate').stat().st_mode & ~0o222)
print(c)

#!/usr/bin/env python3
"""Trace only the two first-call extra equivalence binaries with existing pins."""
import json
from pathlib import Path
import shutil
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
old=root/'benchmarks/runs/20260909-6516-forward-before'
c=root/'benchmarks/runs/20260909-6522-equivalence-life'; assert not c.exists()
p=old/'comparison-6518.json'; s=json.loads(p.read_text()); assert s['parent_replay']
assert s['first_call_large']['before']['identities_and_values']['equal_multiset']
assert s['binary_comparisons'][1]['left_only_examples']==[[[-91779,91609],1],[[-91609,91779],1]]
m=json.loads((old/'manifest.json').read_text())
for rel,digest in m['hashes'].items(): assert sha(old/rel)==digest,rel
for name in ('pins','logs','traces','results'): (c/name).mkdir(parents=True)
for arm in ('candidate','kissat'): shutil.copy2(old/'pins'/arm,c/'pins'/arm)
runner=(old/'run.py').read_text().replace('tactix-6516-before-','tactix-6522-')
anchor="assert not any(key.endswith('_EVENT_TRACE') for key in env)"; assert runner.count(anchor)==1
extra="""env[prefix+'_BINARY_LIFECYCLE_TRACE']=str(c/'traces'/f'{arm}.equivalence-life.tsv')
env[prefix+'_BINARY_LIFECYCLE_PAIRS']='-91779,91609;-91609,91779'
"""
runner=runner.replace(anchor,extra+anchor); compile(runner,str(c/'run.py'),'exec')
(c/'run.py').write_text(runner)
(c/'array.sbatch').write_text((old/'array.sbatch').read_text().replace(str(old),str(c)).replace('6516-forward-before','6522-equivalence-life'))
m.update(parent_summary=dict(path=str(p.relative_to(root)),sha256=sha(p)),
    parent_runs={a:str(old.relative_to(root)) for a in ('candidate','kissat')},
    purpose='Identify the actual producer and copy lifecycle of91609=91779, present only in first-call Tactix output. Stronger local inference is not automatically a defect; do not weaken matching to force native trajectory.',
    validation='All own6516-before traces including large formula and forward binary snapshots plus partial-proof size/hash must match. Parse the existing per-solver lifecycle schemas strictly, isolate conflict227617 and locate producer labels.',
    test_consideration='No source changes, existing immutable tested pins.117bench tests cover snapshots/formulas; lifecycle schema adds focused positive/negative fixtures before interpretation. Full own replay validates observer neutrality.')
m['hashes']={str(q.relative_to(c)):sha(q) for q in [*(c/'pins').iterdir(),c/'run.py',c/'array.sbatch']}
(c/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
for q in (c/'pins').iterdir(): q.chmod(q.stat().st_mode & ~0o222)
print(c)

#!/usr/bin/env python3
"""Actual post-install snapshots and first-call large input, own1M neutral replay."""
import json
from pathlib import Path
import shutil
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
old=root/'benchmarks/runs/20260909-6501-rank33-congruence-replay'
c=root/'benchmarks/runs/20260909-6510-rank33-congruence-stages'
assert not c.exists()
authority=root/'benchmarks/runs/20260909-6508-congruence-stages-build/manifest.json'
a=json.loads(authority.read_text()); assert a['release_exact_parent']
for rel,digest in a['binaries'].items(): assert sha(root/rel)==digest,rel
m=json.loads((old/'manifest.json').read_text())
for rel,digest in m['hashes'].items(): assert sha(old/rel)==digest,rel
for name in ('pins','logs','traces','results'): (c/name).mkdir(parents=True)
shutil.copy2(root/'build/pin-6508-congruence-stages-trace/tactix',c/'pins/candidate')
shutil.copy2(old/'pins/kissat',c/'pins/kissat')
runner=(old/'run.py').read_text().replace('tactix-6501-','tactix-6510-')
anchor="assert not any(key.endswith('_EVENT_TRACE') for key in env)"; assert runner.count(anchor)==1
extra="""env['SAT_EVENT_TRACE_FORMULA_SET_PATH']=str(c/'traces'/f'{arm}.formula.tsv')
env['SAT_EVENT_TRACE_FORMULA_SET_MATCH']=m['formula_selectors'][arm]
env['SAT_EVENT_TRACE_FORMULA_SET_CONFLICT']=str(m['diagnostic_conflict'])
env['SAT_EVENT_TRACE_FORMULA_SET_ORDINAL']='0'
env['SAT_EVENT_TRACE_FORMULA_SET_POSE']='1'
env['SAT_EVENT_TRACE_FORMULA_SET_VALUES']='1'
"""
runner=runner.replace(anchor,extra+anchor); compile(runner,str(c/'run.py'),'exec')
(c/'run.py').write_text(runner)
batch=(old/'array.sbatch').read_text().replace(str(old),str(c)).replace('6501-rank33-congruence','6510-rank33-stages')
assert '--array=0-1' in batch and "arm = ('candidate', 'kissat')" in runner
(c/'array.sbatch').write_text(batch)
m['resource_contract']['array']='0-1'
m['authorities']['candidate']=dict(path=str(authority.relative_to(root)),sha256=sha(authority))
m.update(parent_summary=dict(path=str((old/'summary.json').relative_to(root)),sha256=sha(old/'summary.json')),
    stage_qualification=dict(path=str((old/'stage-qualification-6507.json').relative_to(root)),sha256=sha(old/'stage-qualification-6507.json')),
    parent_runs={arm:str(old.relative_to(root)) for arm in ('candidate','kissat')},
    formula_selectors=dict(candidate='name=congruence-dense-resume|stage=entry',kissat='congruence_dense_entry'),
    purpose='Observe actual installed/gates-built Tactix binaries and full first-call incoming large clause identities, poses and root values at227617. Do not assume gate-exit semantic alignment.',
    validation='Require full ordinary1M traces and partial-proof byte/hash identity against own6501; candidate census exact; old candidate binary census exact after dropping whole installed/gates-built frames. Strict new readers and full incoming formula multiset comparison.',
    test_consideration='6508 actual rule selected/disabled/wrong-conflict state+proof fixture passes;109Release108trace; ordinary Release exact6424. Nine new reader tests for stage and formula framing/identity failures.',
    limitation='Diagnostic only; no final-proof acceptance or timing claim. Existing production, baseline, campaigns and checker scratch unchanged.')
m['hashes']={str(q.relative_to(c)):sha(q) for q in [*(c/'pins').iterdir(),c/'run.py',c/'array.sbatch']}
(c/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
for q in (c/'pins').iterdir(): q.chmod(q.stat().st_mode & ~0o222)
print(c)

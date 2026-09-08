#!/usr/bin/env python3
"""Compare actual live clauses after Rank33's earliest portable count split."""
from pathlib import Path
import datetime
import hashlib
import json
import shutil
root=Path(__file__).resolve().parents[3]
old=root/'benchmarks/runs/20260908-5558-rank33-small-prefix'
c=root/'benchmarks/runs/20260908-5582-rank33-congruence-output'
authority=root/'benchmarks/runs/20260908-5567-rank33-small-extended/nonbinary-localization.json'
def sha(p):
    h=hashlib.sha256()
    with p.open('rb') as f:
        for b in iter(lambda:f.read(1048576),b''):h.update(b)
    return h.hexdigest()
boundary=json.loads(authority.read_text())['first_nonbinary_boundary']
assert boundary['candidate']['name']=='Congruence' and boundary['candidate']['conflicts']==9457
m=json.loads((old/'manifest.json').read_text())
for name,digest in m['hashes'].items():assert sha(old/name)==digest
runner=(old/'run.py').read_text().replace('tactix-5558-','tactix-5582-')
anchor='\ndef interrupted(signum, frame):'
assert runner.count(anchor)==1
runner=runner.replace(anchor,'''
selector = 'congruence_end' if arm == 'kissat' else 'name=Congruence|stage=end'
env.update({'SAT_EVENT_TRACE_FORMULA_SET_PATH': str(c / 'traces' / (arm + '.formula.tsv')),
            'SAT_EVENT_TRACE_FORMULA_SET_MATCH': selector,
            'SAT_EVENT_TRACE_FORMULA_SET_CONFLICT': '9457',
            'SAT_EVENT_TRACE_BINARY_SET_PATH': str(c / 'traces' / (arm + '.binary.tsv')),
            'SAT_EVENT_TRACE_BINARY_SET_MATCH': selector,
            'SAT_EVENT_TRACE_BINARY_SET_CONFLICT': '9457'})
assert not any(key.endswith('_EVENT_TRACE') for key in env)
'''+anchor)
compile(runner,str(c/'run.py'),'exec')
c.mkdir(exist_ok=False)
for name in ('pins','logs','traces','results'):(c/name).mkdir()
for arm in ('candidate','kissat'):shutil.copy2(old/'pins'/arm,c/'pins'/arm)
(c/'run.py').write_text(runner)
(c/'array.sbatch').write_text((old/'array.sbatch').read_text().replace(str(old),str(c)))
m.update(parent_campaign=str(old.relative_to(root)),parent_summary_sha256=sha(old/'summary.json'),
    created_at=datetime.datetime.now(datetime.timezone.utc).isoformat(),
    selected_authority=dict(path=str(authority.relative_to(root)),sha256=sha(authority)),
    hypothesis='Congruence9457 leaves five more live irredundant clauses in5522 while all decisions agree through100K. Compare actual large and binary multisets including duplicate counts before attributing the later182093 choice to this difference.',
    validation='Two clean100K stops, full per-arm5558 decisions/prefix/boundary replay and partial-proof SHA. Full clause rows and framed binary snapshots; preserve multiplicity. No performance or stronger-rule claim.',
    test_consideration='No solver/observer change. Existing formula/snapshot parser fixtures plus strict full replay; new selector-bound fixture distinguishes Congruence output from Factor input and checks native/candidate formats.')
m['hashes']={str(p.relative_to(c)):sha(p) for p in [*(c/'pins').iterdir(),c/'run.py',c/'array.sbatch']}
(c/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
for p in (c/'pins').iterdir():p.chmod(p.stat().st_mode&~0o222)
print(c)

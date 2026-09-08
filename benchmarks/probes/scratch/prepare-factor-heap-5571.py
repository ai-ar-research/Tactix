#!/usr/bin/env python3
"""Bind a standalone heap snapshot immediately before the first unequal grid."""
from pathlib import Path
import datetime
import hashlib
import json
import shutil
root = Path(__file__).resolve().parents[3]
old = root / 'benchmarks/runs/20260908-5537-rank32-small-prefix'
c = root / 'benchmarks/runs/20260908-5571-rank32-factor-heap'
authority = root / 'benchmarks/runs/20260908-5564-rank32-small-factor/factor-input-localization.json'
def sha(p):
    h = hashlib.sha256()
    with p.open('rb') as f:
        for block in iter(lambda:f.read(1048576),b''): h.update(block)
    return h.hexdigest()
evidence = json.loads(authority.read_text())
assert evidence['parent_replay']
split = evidence['first_grid_difference']
assert split['candidate']['ordinal'] == split['kissat']['ordinal'] == 4825
ordinal = split['candidate']['ordinal'] - 1
m = json.loads((old/'manifest.json').read_text())
for name,digest in m['hashes'].items(): assert sha(old/name) == digest
runner = (old/'run.py').read_text().replace('tactix-5537-', 'tactix-5571-')
anchor = '\ndef interrupted(signum, frame):'
assert runner.count(anchor) == 1
runner = runner.replace(anchor, '''
env.update({'SAT_EVENT_TRACE_FACTOR_HEAP_SET_PATH': str(c / 'traces' / (arm + '.heap.tsv')),
            'SAT_EVENT_TRACE_FACTOR_HEAP_SET_STAGE': 'updated',
            'SAT_EVENT_TRACE_FACTOR_HEAP_ORDINAL': str(m['factor_ordinal'])})
assert not any(key.endswith('_EVENT_TRACE') for key in env)
''' + anchor)
compile(runner,str(c/'run.py'),'exec')
c.mkdir(exist_ok=False)
for name in ('pins','logs','traces','results'): (c/name).mkdir()
for arm in ('candidate','kissat'): shutil.copy2(old/'pins'/arm,c/'pins'/arm)
(c/'run.py').write_text(runner)
(c/'array.sbatch').write_text((old/'array.sbatch').read_text().replace(str(old),str(c)))
m.update(parent_campaign=str(old.relative_to(root)), parent_summary_sha256=sha(old/'summary.json'),
    created_at=datetime.datetime.now(datetime.timezone.utc).isoformat(), factor_ordinal=ordinal,
    selected_authority=dict(path=str(authority.relative_to(root)),sha256=sha(authority)),
    hypothesis='The first4824 accepted grids match;4825 selects different equal-gain seeds. Compare actual post4824 heap membership, scores and physical topology before attributing the split to candidate admission, scores or tie ordering.',
    validation='Standalone immutable observers without broad EventTrace. Two clean100000 stops, full per-arm5537 trace replay and partial-proof SHA, exact pin/input/dump hashes. Heap framing, unique membership, contiguous positions and max-heap invariant; no runtime claim.',
    test_consideration='No solver change. Add positive/negative heap-reader fixtures; completed parent replay proves observational neutrality for the selected ordinal.')
m['hashes'] = {str(p.relative_to(c)):sha(p) for p in [*(c/'pins').iterdir(),c/'run.py',c/'array.sbatch']}
(c/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
for p in (c/'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
print(c)

#!/usr/bin/env python3
"""Profile the clean repaired Rank-35 candidate and pristine Kissat."""
import csv
import datetime
import hashlib
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
old = root / 'benchmarks/runs/20260907-5423-rephase-root-ranks1-40'
c = root / 'benchmarks/runs/20260907-5425-rephase-root-profile'
c.mkdir(exist_ok=False)
for d in ('pins', 'logs', 'results', 'provisional', 'claims', 'profiles'): (c / d).mkdir()
for target, source in [('candidate', 'tactix'), ('kissat', 'kissat'), ('candidate.patch', 'candidate-source.patch'),
                       ('runtime_task.py', 'runtime_task.py'), ('checkmodel', 'checkmodel'),
                       ('modelcheck.py', 'modelcheck.py'), ('drat-trim', 'drat-trim')]:
    shutil.copy2(old / 'pins' / source, c / 'pins' / target)
shutil.copy2(root / 'benchmarks/probes/scratch/release-profile-task-5385.py', c / 'pins/run.py')
with (old / 'matrix.tsv').open(newline='') as f:
    reader = csv.DictReader(f, delimiter='\t'); fields = reader.fieldnames
    rows = [r for r in reader if r['rank'] == '35']
assert len(rows) == 2
for i, row in enumerate(rows):
    row['task'] = str(i)
    if row['arm'] == 'tactix': row['arm'] = 'candidate'
with (c / 'matrix.tsv').open('w', newline='') as f:
    w = csv.DictWriter(f, fields, delimiter='\t'); w.writeheader(); w.writerows(rows)
(c / 'array.sbatch').write_text((old / 'array.sbatch').read_text().replace(str(old), str(c)).replace('--array=0-79', '--array=0-1').replace('/pins/runtime_task.py', '/pins/run.py'))
m = json.loads((old / 'manifest.json').read_text())
m['arms']['candidate'] = m['arms'].pop('tactix')
m['arms']['candidate'].update(path='pins/candidate', source_patch='pins/candidate.patch')
m['resource_contract']['array'] = '0-1'
m['input_sha256'] = {rows[0]['instance']: m['input_sha256'][rows[0]['instance']]}
m.update(created_at=datetime.datetime.now(datetime.timezone.utc).isoformat(),
    parent_campaign=str(old.relative_to(root)),
    purpose='Profile clean Release after the causal Rephase repair; distinguish remaining execution-cost hotspots from the changed search trajectory.',
    hypothesis='The repaired Rank-35 candidate performs fewer conflicts/decisions than Kissat but remains slower; identify substantial avoidable assignment, propagation or maintenance costs before choosing an ablation.',
    validation='Expected solver verdicts and usable profiles may guide work while checks run. Final records require both proofs checked and scratch deleted; work counters must reproduce each arm of 5421. Sampling times are diagnostic only, never acceptance.',
    profile=dict(event='cycles:u', frequency=199, call_graph='dwarf,8192'),
    collect_runtime_context=True)
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
m['pin_sha256'] = {str(p.relative_to(c)): sha(p) for p in [*(c / 'pins').iterdir(), c / 'matrix.tsv', c / 'array.sbatch']}
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
for p in (c / 'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
print(c)

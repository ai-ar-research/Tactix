#!/usr/bin/env python3
"""Profile clean, checked full solves; exclude conflict-stop observer overhead."""
import csv
import hashlib
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
old = root / 'benchmarks/runs/20260907-5379-vivify-retry-runtime'
c = root / 'benchmarks/runs/20260907-5385-release-profile'
c.mkdir(exist_ok=False)
for d in ('pins', 'logs', 'results', 'provisional', 'claims', 'profiles'): (c / d).mkdir()
for n in ('candidate', 'kissat', 'candidate.patch', 'runtime_task.py', 'checkmodel', 'modelcheck.py', 'drat-trim'):
    shutil.copy2(old / 'pins' / n, c / 'pins' / n)
shutil.copy2(root / 'benchmarks/probes/scratch/release-profile-task-5385.py', c / 'pins/run.py')
with (old / 'matrix.tsv').open(newline='') as source:
    reader = csv.DictReader(source, delimiter='\t'); fields = reader.fieldnames
    rows = [r for r in reader if r['arm'] in ('candidate', 'kissat')]
assert len(rows) == 6
for i, row in enumerate(rows): row['task'] = str(i)
with (c / 'matrix.tsv').open('w', newline='') as target:
    writer = csv.DictWriter(target, fields, delimiter='\t'); writer.writeheader(); writer.writerows(rows)
s = (old / 'array.sbatch').read_text().replace(str(old), str(c)).replace('--array=0-11', '--array=0-5').replace('/pins/runtime_task.py', '/pins/run.py')
(c / 'array.sbatch').write_text(s)
m = json.loads((old / 'manifest.json').read_text())
m.pop('slurm_job_id', None)
m['arms'] = {a: m['arms'][a] for a in ('candidate', 'kissat')}
m.update(parent_campaign=str(old.relative_to(root)),
         hypothesis='Localize clean Release search/inprocessing hotspots on Ranks 17/21/34; the stopped build includes dormant diagnostic calls and is not a pure production-cost proxy.',
         decision='Six checked SAT models and clean scratch; usable profiles. Sampling timings are diagnostic only, not acceptance; full solve trajectories differ.',
         profile=dict(event='cycles:u', frequency=199, call_graph='dwarf,8192'),
         collect_runtime_context=True)
m['pin_sha256'] = {str(p.relative_to(c)): hashlib.sha256(p.read_bytes()).hexdigest()
                   for p in [*(c / 'pins').iterdir(), c / 'matrix.tsv', c / 'array.sbatch']}
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
for p in (c / 'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
print(c)

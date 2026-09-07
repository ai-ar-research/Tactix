#!/usr/bin/env python3
"""Measure existing slot-memory ownership on the high-RSS Rank-21 witness."""
import csv
import hashlib
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
old = root / 'benchmarks/runs/20260907-5379-vivify-retry-runtime'
c = root / 'benchmarks/runs/20260907-5386-memory-ownership'
c.mkdir(exist_ok=False)
for d in ('pins', 'logs', 'results', 'provisional', 'claims'): (c / d).mkdir()
for n in ('candidate.patch', 'runtime_task.py', 'checkmodel', 'modelcheck.py', 'drat-trim'):
    shutil.copy2(old / 'pins' / n, c / 'pins' / n)
shutil.copy2(root / 'build/pin-5378-vivify-retry/tactix', c / 'pins/candidate')
shutil.copy2(root / 'benchmarks/probes/scratch/memory-ownership-task-5386.py', c / 'pins/run.py')
with (old / 'matrix.tsv').open(newline='') as source:
    reader = csv.DictReader(source, delimiter='\t'); fields = reader.fieldnames
    rows = [r for r in reader if r['arm'] == 'candidate' and r['rank'] == '21']
assert len(rows) == 1; rows[0]['task'] = '0'
with (c / 'matrix.tsv').open('w', newline='') as target:
    writer = csv.DictWriter(target, fields, delimiter='\t'); writer.writeheader(); writer.writerows(rows)
s = (old / 'array.sbatch').read_text().replace(str(old), str(c)).replace('--array=0-11', '--array=0-0').replace('/pins/runtime_task.py', '/pins/run.py')
(c / 'array.sbatch').write_text(s)
m = json.loads((old / 'manifest.json').read_text()); m.pop('slurm_job_id', None)
m['arms'] = {'candidate': m['arms']['candidate']}
m['arms']['candidate']['sha256'] = hashlib.sha256((c / 'pins/candidate').read_bytes()).hexdigest()
m['arms']['candidate']['source_build'] = 'build/pin-5378-vivify-retry'
m.update(parent_campaign=str(old.relative_to(root)),
         hypothesis='The 4.7-GB Release Rank-21 peak versus 1.4-GB Kissat warrants ownership attribution: distinguish dense/index backing, semantic formula/search state and allocator retention.',
         decision='One independently checked SAT model, matching Release logical work, clean scratch and retained slot-memory rows. Read-only attribution, not timing or production memory acceptance.',
         limitations='Uses existing event-trace/conflict-stop-enabled 5378 pin with only slot-memory output selected and no conflict stop. Diagnostic audits can add work/temporary storage; compare counters and phase ownership, do not equate traced peak with Release. Rule-local scratch is not exhaustively inventoried.',
         collect_runtime_context=True)
m['pin_sha256'] = {str(p.relative_to(c)): hashlib.sha256(p.read_bytes()).hexdigest()
                   for p in [*(c / 'pins').iterdir(), c / 'matrix.tsv', c / 'array.sbatch']}
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
for p in (c / 'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
print(c)

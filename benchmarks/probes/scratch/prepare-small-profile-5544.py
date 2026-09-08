#!/usr/bin/env python3
"""Profile the latest implementation on a predeclared mixed work/cost loss."""
import csv
import datetime
import hashlib
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
runs = root / 'benchmarks/runs'
old = runs / '20260908-5535-sweep-small-ranks1-40'
protocol = runs / '20260907-5425-rephase-root-profile'
selection = runs / '20260908-5485-sweep-cursor-ranks1-40/next-losses.json'
c = runs / '20260908-5544-small-rank39-profile'
def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()
m = json.loads((old / 'manifest.json').read_text())
for rel, digest in m['pin_sha256'].items():
    assert sha(old / rel) == digest
pm = json.loads((protocol / 'manifest.json').read_text())
assert sha(protocol / 'pins/run.py') == pm['pin_sha256']['pins/run.py']
with (old / 'matrix.tsv').open(newline='') as f:
    reader = csv.DictReader(f, delimiter='\t')
    fields = reader.fieldnames
    rows = [r for r in reader if r['rank'] == '39']
assert len(rows) == 2 and [r['arm'] for r in rows] == ['tactix', 'kissat']
c.mkdir(exist_ok=False)
for d in ('pins', 'logs', 'results', 'provisional', 'claims', 'profiles'):
    (c / d).mkdir()
for target, source in [('candidate', 'tactix'), ('kissat', 'kissat'),
                       ('candidate.patch', 'candidate-source.patch'),
                       ('runtime_task.py', 'runtime_task.py'), ('checkmodel', 'checkmodel'),
                       ('modelcheck.py', 'modelcheck.py'), ('drat-trim', 'drat-trim')]:
    shutil.copy2(old / 'pins' / source, c / 'pins' / target)
shutil.copy2(protocol / 'pins/run.py', c / 'pins/run.py')
for i, row in enumerate(rows):
    row['task'] = str(i)
    if row['arm'] == 'tactix':
        row['arm'] = 'candidate'
with (c / 'matrix.tsv').open('w', newline='') as f:
    writer = csv.DictWriter(f, fields, delimiter='\t')
    writer.writeheader()
    writer.writerows(rows)
(c / 'array.sbatch').write_text((old / 'array.sbatch').read_text().replace(str(old), str(c))
    .replace('--array=0-79', '--array=0-1').replace('/pins/runtime_task.py', '/pins/run.py'))
m['arms']['candidate'] = m['arms'].pop('tactix')
m['arms']['candidate'].update(path='pins/candidate', source_patch='pins/candidate.patch')
m['resource_contract']['array'] = '0-1'
m['input_sha256'] = {rows[0]['instance']: m['input_sha256'][rows[0]['instance']]}
m.update(created_at=datetime.datetime.now(datetime.timezone.utc).isoformat(),
    parent_campaign=str(old.relative_to(root)),
    purpose='Clean Release5522/native Rank39 profiles to rank remaining unit-cost and inprocessing hypotheses.',
    hypothesis='Prior5485 Rank39 is1.197019x Kissat despite fewer propagations and decisions. Determine which current5522 search/inprocessing costs warrant a source-backed ablation; neither unmatched counters nor samples alone prove unit cost.',
    selection_authority=dict(path=str(selection.relative_to(root)), sha256=sha(selection)),
    validation='Expected verdicts, immutable pin/report/profile hashes, zero lost samples and exact within-arm proof identity plus work counters against unprofiled5535 Rank39. Pending proofs permit investigation, not final acceptance. Instrumented runtime is never an acceptance comparison.',
    test_consideration='No solver or profiler logic changes: reuse tested profile wrapper and runtime runner. Validate parent hashes, exact two-arm rank39 matrix projection and existing model/proof/cleanup oracles; no new solver unit test needed.',
    profile=dict(event='cycles:u', frequency=199, call_graph='dwarf,8192'),
    collect_runtime_context=True)
m.pop('slurm_job_id', None)
m['pin_sha256'] = {str(p.relative_to(c)): sha(p) for p in
                  [*(c / 'pins').iterdir(), c / 'matrix.tsv', c / 'array.sbatch']}
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
for p in (c / 'pins').iterdir():
    p.chmod(p.stat().st_mode & ~0o222)
print(c)

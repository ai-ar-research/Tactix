#!/usr/bin/env python3
"""Pin one small, checked investigation of original-core unit exposure."""
import csv
import datetime as dt
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

root = Path(__file__).resolve().parents[3]
old = root / 'benchmarks/runs/20260907-5314-pose-r2-ranks1-40'
new = root / 'benchmarks/runs/20260907-5318-core-unit-small'
new.mkdir(exist_ok=False)
for name in ('pins', 'logs', 'results', 'provisional', 'claims'):
    (new / name).mkdir()
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
head = subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip()
assert head == 'c2a95a6c79c21a76516cf556b6538e76da369c34'
shutil.copy2(old / 'pins/tactix', new / 'pins/shipped')
shutil.copy2(root / 'build/pin-5318-core-release/tactix', new / 'pins/candidate')
assert sha(new / 'pins/candidate') == 'd6701f2e4c158b056f13956c4f02acf12b38c5c81805757c6066bdd56ef4b72a'
assert sha(new / 'pins/shipped') == 'de3d9332f70f3bbd5ca17fedda4ad11c7295114e8b581251192746ff8d68439f'
for name in ('checkmodel', 'drat-trim', 'modelcheck.py', 'runtime_task.py'):
    shutil.copy2(old / 'pins' / name, new / 'pins' / name)
shutil.copy2(root / 'benchmarks/probes/scratch/core-unit-release-5318.patch',
             new / 'pins/candidate.patch')
with (old / 'matrix.tsv').open(newline='') as source:
    rows = list(csv.DictReader(source, delimiter='\t'))
selected = [row for row in rows if int(row['rank']) in (17, 21, 34) and row['arm'] == 'tactix']
assert len(selected) == 3 and all(row['expected'] == 'SAT' for row in selected)
with (new / 'matrix.tsv').open('w', newline='') as target:
    writer = csv.DictWriter(target, fieldnames=rows[0].keys(), delimiter='\t')
    writer.writeheader()
    for i, row in enumerate(selected):
        for j, arm in enumerate(('shipped', 'candidate')):
            writer.writerow(dict(row, task=2*i+j, arm=arm))
batch = (old / 'array.sbatch').read_text().replace(str(old), str(new)).replace('--array=0-79', '--array=0-5')
(new / 'array.sbatch').write_text(batch)
previous = json.loads((old / 'manifest.json').read_text())
arms = {arm: dict(path=f'pins/{arm}', sha256=sha(new / 'pins' / arm),
                 source_commit=head, arguments=[]) for arm in ('shipped', 'candidate')}
arms['candidate'].update(source_patch='pins/candidate.patch', source_patch_sha256=sha(new / 'pins/candidate.patch'))
manifest = dict(
    schema=previous['schema'], created_at=dt.datetime.now(dt.timezone.utc).isoformat(),
    status='UNFUZZED isolated investigation; no production promotion',
    hypothesis='True ordered core exposure at SAVE/ADD closes the Rank-34 boundary and reduces downstream work without material component overhead.',
    comparison='Current fuzz-passing r2 versus isolated original-core candidate; proof on both; Ranks 17, 21, 34 from the unchanged parent matrix.',
    decision='All six models must check and scratch be clean. A single wave is diagnostic: use work and runtime to choose the next test, not a decision-grade speedup claim.',
    arms=arms, collect_runtime_context=True,
    input_sha256={r['instance']: previous['input_sha256'][r['instance']] for r in selected},
    resource_contract=dict(previous['resource_contract'], array='0-5'),
    pin_sha256={str(p.relative_to(new)): sha(p) for p in (new / 'pins').iterdir()},
    parent_campaign=str(old.relative_to(root)))
for name in ('matrix.tsv', 'array.sbatch'):
    manifest['pin_sha256'][name] = sha(new / name)
(new / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
for p in (new / 'pins').iterdir():
    p.chmod(p.stat().st_mode & ~0o222)
print(new)

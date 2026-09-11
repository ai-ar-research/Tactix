#!/usr/bin/env python3
"""Freeze three matched Factor-arena waves, preserving the existing runner."""
import csv
import datetime as dt
import importlib.util
import json
from pathlib import Path
import shutil
import subprocess
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify

c = root / 'benchmarks/runs/20260910-7186-factor-arena-runtime'
assert not c.exists()
gate = root / 'benchmarks/runs/20260910-7183-factor-arena-BC'
subprocess.run([sys.executable, str(Path(__file__).with_name('check-submit-factor-arena-BC-7183.py'))], check=True)
reader = Path(__file__).with_name('submit-vivify-compact-wave-7150.py')
spec = importlib.util.spec_from_file_location('fingerprints', reader)
h = importlib.util.module_from_spec(spec); spec.loader.exec_module(h); h.fixtures()
gm = json.loads((gate / 'manifest.json').read_text())
dependencies = dict(gm['dependencies'])
def bind(p, expected=None):
    digest = sha(p); assert expected is None or digest == expected, p
    dependencies[str(p.relative_to(root))] = digest
for rel, digest in list(dependencies.items()): bind(root / rel, digest)
for rel, digest in gm['pin_sha256'].items(): bind(gate / rel, digest)
bind(gate / 'manifest.json'); bind(gate / 'launch.json'); bind(reader); bind(Path(__file__))
records = {}
for task in range(66):
    p = gate / f'results/{task:03d}.json'; final = p.exists()
    if not final: p = gate / f'provisional/{task:03d}.json'
    r = json.loads(p.read_text())
    assert classify(r, final)['status'] in ('pending_validation', 'checked_SAT', 'checked_UNSAT')
    bind(p); records[task] = r
for task in range(0, 66, 2): assert h.fingerprint(records[task]) == h.fingerprint(records[task+1])
full = root / 'benchmarks/runs/20260910-7167-vivify-revalidation-ranks1-50'
fm = json.loads((full / 'manifest.json').read_text()); bind(full / 'manifest.json')
for folder in ('provisional', 'results'):
    for p in (full / folder).glob('*.json'):
        assert classify(json.loads(p.read_text()), folder == 'results')['status'] in ('pending_validation', 'checked_SAT', 'checked_UNSAT')
rows, oracles = [], {}
for wave in range(3):
    for rank in (31, 35, 38):
        p = full / f'results/{rank-1:03d}.json'
        if not p.exists(): p = full / f'provisional/{rank-1:03d}.json'
        r = json.loads(p.read_text()); assert r['verdict'] == r['expected'] == 'UNSAT'
        bind(p); oracles[str(rank)] = dict(path=str(p.relative_to(root)), sha256=sha(p))
        for arm in (('previous', 'candidate') if wave == 1 else ('candidate', 'previous')):
            rows.append(dict(task=len(rows), rank=rank, arm=arm, tier='D', instance=r['instance'], expected='UNSAT'))
for folder in ('pins', 'logs', 'claims', 'provisional', 'results'): (c / folder).mkdir(parents=True)
for name in ('runtime_task.py', 'drat-trim', 'checkmodel', 'modelcheck.py'):
    shutil.copy2(gate / 'pins' / name, c / 'pins' / name)
shutil.copy2(reader, c / 'pins/exact_fingerprint.py')
arms = {}
for arm, old in (('previous', 'current_B'), ('candidate', 'candidate_B')):
    info = dict(gm['arms'][old]); info['path'] = 'pins/' + arm
    arms[arm] = info
    shutil.copy2(gate / gm['arms'][old]['path'], c / info['path'])
    assert sha(c / info['path']) == info['sha256']
with (c / 'matrix.tsv').open('x', newline='') as out:
    w = csv.DictWriter(out, fieldnames=rows[0], delimiter='\t', lineterminator='\n')
    w.writeheader(); w.writerows(rows)
batch = (gate / 'array.sbatch').read_text().replace(str(gate), str(c)).replace('7183-factor-arena-BC', '7186-factor-arena-runtime').replace('--array=0-65', '--array=0-17')
with (c / 'array.sbatch').open('x') as out: out.write(batch)
m = dict(schema='tactix-factor-arena-runtime-v1', created_at=dt.datetime.now(dt.timezone.utc).isoformat(),
    arms=arms, input_sha256={r['instance']: fm['input_sha256'][r['instance']] for r in rows},
    collect_runtime_context=True, resource_contract=dict(gm['resource_contract'], array='0-17'),
    dependencies=dependencies, previous_work_oracles=oracles, hypothesis=gm['hypothesis'],
    test_consideration='Frozen7150 exact fingerprint fixtures plus new7186 matrix/identity controls; underlying arena unit/proof/sanitizer gates bound through7183. No changes to solver or copied runtime runner.',
    acceptance='Every outcome expected, every non-timing statistic and certificate bytes/hash exact retained7144 and own repeats. Independent proof/model checks and zero residue required for final correctness. Pending checks permit independent work only.',
    measurement='Three independent paired31/35/38 six-task waves, reversed middle order.12 logicalCPUs/48000MiB/1200s solver-only,16000s checker,5h allocation. Fullwidth/no nodeconstraint. Paired medians/full ranges/RSS; absolute headline requires each complete arm spread<=2%. No arbitrary minimum gain.',
    decision='Measure whole runtime and memory including arena holes. Complete Factor profiles are also required for structural unit-cost attribution. Normal telemetry retained. No production integration, default promotion or fuzz waiver.')
m['pin_sha256'] = {str(p.relative_to(c)): sha(p) for p in [*(c / 'pins').iterdir(), c / 'matrix.tsv', c / 'array.sbatch']}
with (c / 'manifest.json').open('x') as out: json.dump(m, out, indent=2)
for p in (c / 'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
print(c)

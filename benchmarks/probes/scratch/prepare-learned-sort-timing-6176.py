#!/usr/bin/env python3
"""Pin three unprofiled Rank4 waves; only the two Tactix implementations run."""
import csv
import datetime as dt
import json
from pathlib import Path
import shutil
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
c = root / 'benchmarks/runs/20260909-6176-learned-sort-rank4-runtime'
assert not c.exists()
profile = root / 'benchmarks/runs/20260909-6166-rank4-release-profile'
pm = json.loads((profile / 'manifest.json').read_text())
build = root / 'benchmarks/runs/20260909-6169-learned-sort-build'
b = json.loads((build / 'manifest.json').read_text())
gate = root / 'benchmarks/runs/20260909-6171-learned-sort-BC-gate'
records = {}
for folder in ('provisional', 'results'):
    for p in (gate / folder).glob('*.json'):
        r = json.loads(p.read_text())
        assert classify(r, folder == 'results')['status'] in ('pending_validation', 'checked_SAT', 'checked_UNSAT')
        records[r['task']] = p
assert set(records) == set(range(66)), 'all BC expected solver results required'
for rel, digest in pm['pin_sha256'].items():
    assert sha(profile / rel) == digest, rel
for rel, digest in b['files'].items():
    assert sha(root / b['source'] / rel) == digest, rel
for rel, digest in b['binaries'].items():
    assert sha(root / rel) == digest, rel
for folder in ('pins', 'logs', 'results', 'provisional', 'claims'):
    (c / folder).mkdir(parents=True)
for name in ('runtime_task.py', 'drat-trim', 'checkmodel', 'modelcheck.py'):
    shutil.copy2(profile / 'pins' / name, c / 'pins' / name)
shutil.copy2(profile / 'pins/candidate', c / 'pins/previous')
shutil.copy2(root / 'build/pin-6169-learned-sort-release/tactix', c / 'pins/candidate')
with (profile / 'matrix.tsv').open(newline='') as stream:
    seed = next(csv.DictReader(stream, delimiter='\t'))
rows = [dict(seed, task=2 * wave + side, arm=arm) for wave in range(3)
        for side, arm in enumerate(('candidate', 'previous') if wave != 1 else ('previous', 'candidate'))]
with (c / 'matrix.tsv').open('x', newline='') as stream:
    writer = csv.DictWriter(stream, fieldnames=rows[0], delimiter='\t', lineterminator='\n')
    writer.writeheader(); writer.writerows(rows)
batch = (profile / 'array.sbatch').read_text().replace(str(profile), str(c))
batch = batch.replace('--array=0-1', '--array=0-5').replace('/pins/run.py', '/pins/runtime_task.py')
batch = batch.replace('6121-fresh-accepted50', '6176-sort-rank4')
(c / 'array.sbatch').write_text(batch)
arms = dict(previous=dict(pm['arms']['candidate'], path='pins/previous'),
    candidate=dict(path='pins/candidate', sha256=sha(c / 'pins/candidate'), arguments=[],
        source_commit=b['source_commit'], source_build='build/pin-6169-learned-sort-release'))
m = dict(schema='tactix-learned-sort-three-waves-v1', created_at=dt.datetime.now(dt.timezone.utc).isoformat(),
    arms=arms, input_sha256=pm['input_sha256'], collect_runtime_context=True,
    resource_contract=dict(pm['resource_contract'], array='0-5'),
    submission_contract='Three sequential waves of concurrent independent array tasks:0-1,2-3,4-5. Advance after expected solver results,not checking. Reverse arm/task order in middle wave; no hard node constraints or throttles.',
    protected_campaign_hashes=pm['protected_campaign_hashes'],
    candidate_authority=dict(path=str((build / 'manifest.json').relative_to(root)), sha256=sha(build / 'manifest.json')),
    previous_authority=pm['current_authority'],
    predecessor_rank4_record=pm['unprofiled_records']['candidate'],
    bc_solver_records={str(p.relative_to(root)): sha(p) for p in records.values()},
    purpose='Test whether constant-byte identity-pass omission reduces ordinary Release Rank4 cost.',
    acceptance='All6 exact predecessor non-timing statistics and proof bytes/hash; independent checks and cleanup; report median and full range of3 paired ratios. Absolute headline requires <=2% per-arm spread. No global gain or promotion from this subset.',
    test_consideration='Unchanged hash-pinned runner/oracles;39 benchmark and13 watcher tests. The candidate has102/102 tests twice and433 actual-helper fixtures. Validate all6 matrix identities and exact full replay at each wave.',
    status='Unfuzzed isolated experiment; no new Kissat run or reference refresh.')
assert [r['task'] for r in rows] == list(range(6))
assert all({r['arm'] for r in rows[2*w:2*w+2]} == {'candidate', 'previous'} for w in range(3))
m['pin_sha256'] = {str(p.relative_to(c)): sha(p) for p in [*(c / 'pins').iterdir(), c / 'matrix.tsv', c / 'array.sbatch']}
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
for p in (c / 'pins').iterdir():
    p.chmod(p.stat().st_mode & ~0o222)
print(c)

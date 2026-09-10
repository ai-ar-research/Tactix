#!/usr/bin/env python3
"""Predeclare three four-task31/35 waves; new work must repeat, not equal parent."""
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
c = root / 'benchmarks/runs/20260910-7041-collect-retry-runtime'
assert not c.exists()
gate = root / 'benchmarks/runs/20260910-7034-collect-retry-BC-gate'
summary = gate / 'summary-7036.json'
g = json.loads(summary.read_text())
assert g['solver_records'] == 66 and len(g['pairs']) == 33
for rel, digest in g['hashes'].items(): assert sha(root / rel) == digest, rel
m = json.loads((gate / 'manifest.json').read_text())
for rel, digest in m['pin_sha256'].items(): assert sha(gate / rel) == digest, rel
full = root / 'benchmarks/runs/20260909-6743-reduce-watermark-ranks1-50'
fm = json.loads((full / 'manifest.json').read_text())
protected = json.loads((full / 'launch.json').read_text())['protected_campaign_hashes']
for rel, digest in protected.items(): assert sha(root / rel) == digest, rel
authorities, arms, oracles, inputs, seeds = {}, {}, {}, {}, {}
for arm, date, stem in [('candidate', '20260910', '7030-collect-retry'),
                         ('previous', '20260909', '6730-reduce-watermark')]:
    path = root / f'benchmarks/runs/{date}-{stem}-build/manifest.json'
    build = json.loads(path.read_text())
    for rel, digest in build['files'].items(): assert sha(root / build['source'] / rel) == digest, rel
    binary = root / f'build/pin-{stem}-release/tactix'
    assert sha(binary) == build['binaries'][str(binary.relative_to(root))]
    authorities[arm] = dict(path=str(path.relative_to(root)), sha256=sha(path))
    arms[arm] = dict(path='pins/' + arm, sha256=sha(binary), arguments=[],
        source_commit=build['source_commit'], source_build=str(binary.parent.relative_to(root)))
for rank in (31, 35):
    path = full / f'results/{rank-1:03d}.json'
    r = json.loads(path.read_text())
    assert classify(r, True)['status'] == 'checked_UNSAT' and r['rank'] == rank
    oracles[str(rank)] = dict(path=str(path.relative_to(root)), sha256=sha(path))
    inputs[r['instance']] = fm['input_sha256'][r['instance']]
    seeds[rank] = dict(rank=rank, tier='D', instance=r['instance'], expected='UNSAT')
for name in ('pins', 'logs', 'results', 'provisional', 'claims'): (c / name).mkdir(parents=True)
for name in ('runtime_task.py', 'drat-trim', 'checkmodel', 'modelcheck.py'):
    shutil.copy2(gate / 'pins' / name, c / 'pins' / name)
for arm, info in arms.items(): shutil.copy2(root / info['source_build'] / 'tactix', c / info['path'])
rows = []
for wave in range(3):
    for rank in (31, 35):
        for arm in (('previous', 'candidate') if wave == 1 else ('candidate', 'previous')):
            rows.append(dict(task=len(rows), arm=arm, **seeds[rank]))
with (c / 'matrix.tsv').open('x', newline='') as stream:
    writer = csv.DictWriter(stream, fieldnames=rows[0], delimiter='\t', lineterminator='\n')
    writer.writeheader(); writer.writerows(rows)
batch = (gate / 'array.sbatch').read_text().replace(str(gate), str(c)).replace(
    '--array=0-65', '--array=0-11').replace('7034-collect-retry-BC', '7041-collect-retry-runtime')
(c / 'array.sbatch').write_text(batch)
dependencies = [summary, full / 'manifest.json', Path(__file__),
    root / 'benchmarks/runs/20260910-7033-collect-retry-proofs/summary.json',
    root / 'benchmarks/runs/20260910-7024-rank35-retry-producer/producer-localization-7032.json']
out = dict(schema='tactix-collect-retry-three-wave-runtime-v1', created_at=dt.datetime.now(dt.timezone.utc).isoformat(),
    arms=arms, authorities=authorities, input_sha256=inputs, collect_runtime_context=True,
    resource_contract=dict(m['resource_contract'], array='0-11'), protected_campaign_hashes=protected,
    previous_work_oracles=oracles, dependencies={str(p.relative_to(root)): sha(p) for p in dependencies},
    witnesses={'31': 'largest absolute gap; transfer/adverse control, no resurrected5920 cleanup',
               '35': 'second largest gap; source-backed excess BVE retry eligibility after collection'},
    submission_contract='Three sequential full-width four-task waves0-3,4-7,8-11. Advance after expected solver outcomes and exact own repetition, never wait for proof checking. Middle wave reverses arms; no node constraint/throttle.',
    hypothesis='Kissat-equivalent satisfied-large collection retry bookkeeping reduces downstream search overhead. Price total solver, work, slot/outside, RSS and transfer effects; trace parity is not runtime acceptance.',
    acceptance='All12 expected independently checked/clean outcomes. Previous must match frozen6730 non-timing stats/certificate; candidate need not match previous but must repeat its own wave0 statistics/certificate. Report all paired ratios, median/range and within-arm spread; absolute claim requires<=2%. No repeat-to-select or fixed50 refresh.',
    test_consideration='113/113/112 suites,40 advice states and parent-negative fixture,16 strong proofs/two negatives,66 B/C expected outcomes. Immutable runtime runner unchanged. New wave checker explicitly tests own-repeat projection versus changed cross-arm work.',
    limitation='Isolated/unfuzzed; no promotion. Full50/memory/consolidation gates remain. Explicit user resumed after old Sep10 06:00 cutoff.')
out['pin_sha256'] = {str(p.relative_to(c)): sha(p) for p in [*(c / 'pins').iterdir(), c / 'matrix.tsv', c / 'array.sbatch']}
with (c / 'manifest.json').open('x') as stream:
    json.dump(out, stream, indent=2); stream.write('\n')
for p in (c / 'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
print(c)

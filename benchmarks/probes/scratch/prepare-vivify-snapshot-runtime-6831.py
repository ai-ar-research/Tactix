#!/usr/bin/env python3
"""Predeclare three same-work waves on broad/Sweep-heavy/low-slot controls."""
import csv
import datetime as dt
import json
from pathlib import Path
import shutil
import subprocess
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root/'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
subprocess.run([sys.executable, str(root/'tools/slurm/session_deadline.py'), '--deadline', '2026-09-10T06:00:00-04:00'], check=True)
c = root/'benchmarks/runs/20260910-6831-vivify-snapshot-runtime'
assert not c.exists()
gate = root/'benchmarks/runs/20260910-6828-vivify-snapshot-BC-gate'
summary = gate/'summary-6830.json'
g = json.loads(summary.read_text())
assert g['solver_records'] == 66 and len(g['pairs']) == 33
assert all(p['reported_work_equal'] and p['certificate_equal'] for p in g['pairs'])
for rel, digest in g['hashes'].items(): assert sha(root/rel) == digest
m = json.loads((gate/'manifest.json').read_text())
for rel, digest in m['pin_sha256'].items(): assert sha(gate/rel) == digest
proof = root/'benchmarks/runs/20260910-6827-vivify-snapshot-proofs/summary.json'
assert json.loads(proof.read_text())['checked'] == 48
full = root/'benchmarks/runs/20260909-6743-reduce-watermark-ranks1-50'
fm = json.loads((full/'manifest.json').read_text())
protected = json.loads((full/'launch.json').read_text())['protected_campaign_hashes']
for rel, digest in protected.items(): assert sha(root/rel) == digest
authorities = {}; arms = {}; oracles = {}; inputs = {}; seeds = {}
for arm, stamp, stem in (('candidate', '20260910', '6822-vivify-snapshot'), ('previous', '20260909', '6730-reduce-watermark')):
    authority = root/f'benchmarks/runs/{stamp}-{stem}-build/manifest.json'
    build = json.loads(authority.read_text())
    for rel, digest in build['files'].items(): assert sha(root/build['source']/rel) == digest
    pin = root/f'build/pin-{stem}-release/tactix'
    assert sha(pin) == build['binaries'][str(pin.relative_to(root))]
    authorities[arm] = dict(path=str(authority.relative_to(root)), sha256=sha(authority))
    arms[arm] = dict(path='pins/'+arm, sha256=sha(pin), arguments=[], source_commit=build['source_commit'], source_build=str(pin.parent.relative_to(root)))
    oracles[arm] = {}
    for rank in (32, 39, 38):
        p = full/f'results/{rank-1:03d}.json'
        r = json.loads(p.read_text())
        assert classify(r, True)['status'] == 'checked_UNSAT' and r['rank'] == rank
        oracles[arm][str(rank)] = dict(path=str(p.relative_to(root)), sha256=sha(p))
        inputs[r['instance']] = fm['input_sha256'][r['instance']]
        seeds[rank] = dict(rank=rank, tier='D', instance=r['instance'], expected='UNSAT')
for name in ('pins', 'logs', 'results', 'provisional', 'claims'): (c/name).mkdir(parents=True)
for name in ('runtime_task.py', 'drat-trim', 'checkmodel', 'modelcheck.py'):
    shutil.copy2(gate/'pins'/name, c/'pins'/name)
for arm, info in arms.items(): shutil.copy2(root/info['source_build']/'tactix', c/info['path'])
rows = []
for wave in range(3):
    for rank in (32, 39, 38):
        for arm in (('previous', 'candidate') if wave == 1 else ('candidate', 'previous')):
            rows.append(dict(task=len(rows), arm=arm, **seeds[rank]))
assert len(rows) == 18 and all({r['arm'] for r in rows[i:i+2]} == {'candidate', 'previous'} for i in range(0, 18, 2))
with (c/'matrix.tsv').open('x', newline='') as out:
    writer = csv.DictWriter(out, fieldnames=rows[0], delimiter='\t', lineterminator='\n')
    writer.writeheader(); writer.writerows(rows)
batch = (gate/'array.sbatch').read_text().replace(str(gate), str(c)).replace('--array=0-65', '--array=0-17').replace('6828-vivify-snapshot-BC', '6831-vivify-snapshot-runtime')
(c/'array.sbatch').write_text(batch)
deps = [summary, proof, full/'manifest.json', root/'benchmarks/runs/20260910-6824-vivify-snapshot-control/control-6824.json', Path(__file__), root/'benchmarks/runs/20260910-6815-inprocessing-release-profiles/solver-summary-6817.json']
out = dict(schema='tactix-vivify-snapshot-three-wave-runtime-v1', created_at=dt.datetime.now(dt.timezone.utc).isoformat(),
    arms=arms, authorities=authorities, input_sha256=inputs, collect_runtime_context=True,
    resource_contract=dict(m['resource_contract'], array='0-17'), protected_campaign_hashes=protected,
    work_oracles=oracles, dependencies={str(p.relative_to(root)):sha(p) for p in deps},
    witnesses={'32':'broad inprocessing exact-native-conflict/decision control', '39':'Sweep-heavy inprocessing transfer control; native work differs', '38':'known code-cost-sensitive low-slot adverse control'},
    submission_contract='Three sequential full-width six-task waves0-5,6-11,12-17; advance after exact solver outcomes, never wait for proofs. Middle wave reverses arms; no node constraints or throttles.',
    hypothesis='Omitting unused pure-irredundant non-flushing Vivify literal snapshots lowers implementation cost without altering work, budgets or CDCL. Price total solver and Vivify timers; retain all adverse outcomes and control38.',
    acceptance='All18 expected independently checked-clean outcomes and exact same6730 per-rank non-timing statistics/certificate bytes/hash. Report every paired ratio, median/range and full within-arm spread; absolute claim requires<=2%. No repeat-to-select noise, fixed50 denominator refresh, promotion or global parity claim.',
    test_consideration='113/113/112 full suites,432+216 actual old/new state/model/proof-digest cases,144 ordering cases and48 independent fixed proofs. All33 B/C pairs exact. Same immutable runtime runner and validated six-task-wave protocol;244 benchmark and19 watcher tests.',
    limitation='Isolated/unfuzzed. Memory/full50/fuzz acceptance remains separate. No FlatLists redesign or denied transfer/push retry. Winddown05:45/cutoff06:00EDT September10.')
out['pin_sha256'] = {str(p.relative_to(c)):sha(p) for p in [*(c/'pins').iterdir(), c/'matrix.tsv', c/'array.sbatch']}
with (c/'manifest.json').open('x') as stream:
    json.dump(out, stream, indent=2); stream.write('\n')
for p in (c/'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
print(c)

#!/usr/bin/env python3
"""Validate full sampled work/proof replay before using inprocessing cycle samples."""
import datetime as dt
import json
from pathlib import Path
import re
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root/'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
from runtime_summary import counters
c = root/'benchmarks/runs/20260910-6815-inprocessing-release-profiles'
target = c/'solver-summary-6817.json'
assert not target.exists(), 'frozen summary exists'
paths = [c/f'results/{task:03d}.json' if (c/f'results/{task:03d}.json').exists()
         else c/f'provisional/{task:03d}.json' for task in range(4)]
if not all(p.exists() for p in paths):
    print('Profile solver results pending; no summary written')
    raise SystemExit(0)
m = json.loads((c/'manifest.json').read_text())
hashes = {}
def bind(p, expected=None):
    digest = sha(p)
    assert expected is None or digest == expected, p
    hashes[str(p.relative_to(root))] = digest
for rel, digest in m['pin_sha256'].items():
    bind(c/rel, digest)
for group in ('dependencies', 'protected_campaign_hashes'):
    for rel, digest in m[group].items():
        bind(root/rel, digest)
for name in ('manifest.json', 'launch.json'):
    bind(c/name)
arms = {}
for task, (rank, arm) in enumerate(((32,'candidate'),(32,'kissat'),(39,'candidate'),(39,'kissat'))):
    p = paths[task]
    bind(p)
    r = json.loads(p.read_text())
    state = classify(r, p.parent.name == 'results')
    assert state['status'] in ('pending_validation', 'checked_UNSAT'), (p, state)
    baseline = m['unprofiled_records'][f'{rank}/{arm}']
    old_path = root/baseline['path']
    bind(old_path, baseline['sha256'])
    old = json.loads(old_path.read_text())
    old_state = classify(old, True)
    assert old_state['checked'] and old_state['clean']
    assert r['task'] == task and r['arm'] == arm and r['rank'] == rank
    for key in ('instance', 'expected', 'verdict', 'certificate_sha256', 'certificate_bytes'):
        assert r[key] == old[key], (arm, key)
    if p.parent.name == 'results':
        assert r['binary_sha256'] == old['binary_sha256'] == m['arms'][arm]['sha256']
    assert counters(r) == counters(old) and all(v is not None for v in counters(r).values())
    non_timing = lambda record: [s for s in record['solver_statistics'] if not s.startswith('c timing')]
    if arm == 'candidate': assert non_timing(r) == non_timing(old), arm
    provisional_path = c/f'provisional/{task:03d}.json'
    bind(provisional_path)
    provisional = json.loads(provisional_path.read_text())
    for key in ('solve', 'certificate_sha256', 'certificate_bytes', 'solver_statistics'):
        assert r[key] == provisional[key], key
    profile = r['solve']['profile']
    assert set(profile['reports']) == {'self', 'callpaths'}
    for name, report in profile['reports'].items():
        assert report['exit'] == 0
        bind(c/f'profiles/{task:03d}-{name}.txt', report['sha256'])
    data = c/f'profiles/{task:03d}.perf.data.xz'
    bind(data, profile['compressed_sha256'])
    assert data.stat().st_size == profile['compressed_bytes'] > 0
    body = (c/f'profiles/{task:03d}-self.txt').read_text()
    assert '# Total Lost Samples: 0' in body
    rows = []
    for line in body.splitlines():
        match = re.match(r'\s*([\d.]+)%\s+(\S+)\s+\[\.\]\s+(.+?)\s+-\s+-\s*$', line)
        if match:
            rows.append(dict(percent=float(match[1]), dso=match[2], symbol=match[3]))
    assert rows and any(row['dso'] == arm for row in rows)
    arms[f'{rank}/{arm}'] = dict(rank=rank, arm=arm, record=str(p.relative_to(root)), state=state, work=counters(r),
        all_non_timing_statistics_exact=(arm == 'candidate'), certificate_sha256=r['certificate_sha256'],
        certificate_bytes=r['certificate_bytes'], self_samples=rows, profile=profile,
        diagnostic_seconds=r['solve']['wall_seconds'])
for p in (Path(__file__), root/'tools/bench/runtime_summary.py', root/'tools/bench/checked_comparison.py'):
    bind(p)
out = dict(observed_at=dt.datetime.now(dt.timezone.utc).isoformat(), hashes=hashes,
    arms=arms, checked_clean=sum(v['state']['checked'] and v['state']['clean'] for v in arms.values()),
    conclusion='All four sampled arms replay own unprofiled counters and certificates; candidates replay every non-timing statistic. Use current inprocessing callpaths to select a same-work implementation optimization while retaining separate scheduling/CDCL parity obligations.',
    limitation='One four-task199Hz cycles:u diagnostic, not repeated timing acceptance. Percentages and differently scoped timers are not unit prices. Rank32 full conflicts/decisions match native, not proof of all-operation identity; Rank39 work differs. No new source promotion or global scheduling/CDCL parity claim; pending checks remain unverified.')
with target.open('x') as stream:
    json.dump(out, stream, indent=2); stream.write('\n')
print(target)
print(json.dumps({arm:record['self_samples'][:8] for arm, record in arms.items()}, indent=2))

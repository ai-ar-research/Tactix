#!/usr/bin/env python3
"""Seal checked Release samples only after exact within-arm replay."""
import datetime as dt
import json
from pathlib import Path
import re
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
from runtime_summary import counters

c = root / 'benchmarks/runs/20260909-6166-rank4-release-profile'
assert not (c / 'summary.json').exists(), 'frozen summary exists'
m = json.loads((c / 'manifest.json').read_text())
for rel, digest in m['pin_sha256'].items():
    assert sha(c / rel) == digest, rel
for rel, digest in m['protected_campaign_hashes'].items():
    assert sha(root / rel) == digest, rel
arms = {}
for task, arm in enumerate(('candidate', 'kissat')):
    path = c / f'results/{task:03d}.json'
    r = json.loads(path.read_text())
    old_path = root / m['unprofiled_records'][arm]['path']
    assert sha(old_path) == m['unprofiled_records'][arm]['sha256']
    old = json.loads(old_path.read_text())
    assert classify(r, True)['checked'] and classify(old, True)['checked']
    assert r['task'] == task and r['arm'] == arm
    for key in ('instance', 'expected', 'verdict', 'binary_sha256',
                'certificate_sha256', 'certificate_bytes'):
        assert r[key] == old[key], (arm, key)
    assert r['binary_sha256'] == m['arms'][arm]['sha256']
    work = counters(r)
    assert all(v is not None for v in work.values()) and work == counters(old)
    non_timing = lambda x: [s for s in x['solver_statistics'] if not s.startswith('c timing')]
    if arm == 'candidate':
        assert non_timing(r) == non_timing(old)
    provisional = json.loads((c / f'provisional/{task:03d}.json').read_text())
    for key in ('solve', 'certificate_sha256', 'certificate_bytes', 'solver_statistics'):
        assert r[key] == provisional[key], key
    profile = r['solve']['profile']
    for name, report in profile['reports'].items():
        assert report['exit'] == 0
        assert report['sha256'] == sha(c / f'profiles/{task:03d}-{name}.txt')
    data = c / f'profiles/{task:03d}.perf.data.xz'
    assert sha(data) == profile['compressed_sha256']
    assert data.stat().st_size == profile['compressed_bytes']
    body = (c / f'profiles/{task:03d}-self.txt').read_text()
    assert '# Total Lost Samples: 0' in body
    rows = []
    for line in body.splitlines():
        match = re.match(r'\s*([\d.]+)%\s+(\S+)\s+\[\.\]\s+(.+?)\s+-\s+-\s*$', line)
        if match:
            rows.append(dict(percent=float(match[1]), dso=match[2], symbol=match[3]))
    assert rows and any(v['dso'] == arm for v in rows)
    arms[arm] = dict(record=str(path.relative_to(root)), record_sha256=sha(path),
        checked_clean=True, work=work, certificate_sha256=r['certificate_sha256'],
        certificate_bytes=r['certificate_bytes'], self_samples=rows,
        profile=profile, non_timing_replay='all statistics' if arm == 'candidate' else 'three counters and exact proof')
result = dict(observed_at=dt.datetime.now(dt.timezone.utc).isoformat(),
    manifest_sha256=sha(c / 'manifest.json'), checked_clean=2, arms=arms,
    conclusion='Rank4 current Release profiles are neutral by exact within-arm work/proof replay. Learned-level radix sorting is a bounded engineering target; no scheduling change or runtime gain is established.',
    limitation='One approximately eight-second sampled pair, not repeated timing acceptance. Cross-solver function/timer and propagation scopes differ; compare neither raw percentages nor propagation counts as equal operations.')
(c / 'summary.json').write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps({a: v['self_samples'][:3] for a, v in arms.items()}, indent=2))

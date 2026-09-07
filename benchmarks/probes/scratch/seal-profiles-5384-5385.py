#!/usr/bin/env python3
"""Validate profile pins, observed work, oracles and cleanup before interpretation."""
import hashlib
import json
from pathlib import Path
import re

root = Path(__file__).resolve().parents[3]
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def counters(rows):
    text = '\n'.join(rows)
    return {n: int(re.search(r'^c ' + n + r'\s*:?\s*(\d+)', text, re.M).group(1))
            for n in ('conflicts', 'decisions', 'propagations')}
def self_profile(p):
    text = p.read_text()
    assert '# Total Lost Samples: 0' in text
    rows = []
    for line in text.splitlines():
        match = re.match(r'\s*([\d.]+)%\s+(\S+)\s+\[\.\]\s+(.+?)\s+-\s+-\s*$', line)
        if match:
            rows.append(dict(percent=float(match[1]), dso=match[2], symbol=match[3]))
    assert rows and any(r['dso'] in ('tactix', 'candidate', 'kissat') for r in rows)
    return rows
def save(c, result):
    p = c / 'summary.json'
    if p.exists(): assert json.loads(p.read_text()) == result
    else: p.write_text(json.dumps(result, indent=2) + '\n')
    print(c.name, result['final_records'])

c = root / 'benchmarks/runs/20260907-5384-prefix-profile'
m = json.loads((c / 'manifest.json').read_text())
for rel, expected in m['hashes'].items(): assert sha(c / rel) == expected
arms = {}
for a in ('tactix', 'kissat'):
    r = json.loads((c / 'results' / (a + '.json')).read_text())
    old = json.loads((root / m['parent_campaign'] / 'results' / (a + '.json')).read_text())
    assert r['scratch_deleted'] and r['verdict'] == 'UNKNOWN'
    assert r['measurement']['exit_code'] == 0 and not r['measurement']['timed_out']
    assert r['binary_sha256'] == m['hashes']['pins/' + a]
    work = counters(r['statistics']); assert work == counters(old['statistics'])
    for n, report in r['reports'].items():
        assert report['exit'] == 0 and report['sha256'] == sha(c / 'profiles' / (a + '-' + n + '.txt'))
    assert sha(c / 'profiles' / (a + '.perf.data.xz')) == r['profile']['compressed_sha256']
    arms[a] = dict(counters=work, self_samples=self_profile(c / 'profiles' / (a + '-self.txt')))
save(c, dict(final_records=2, arms=arms,
             result_sha256={p.name: sha(p) for p in sorted((c / 'results').glob('*.json'))},
             conclusion='Actual stops and counters reproduce 5382. Dormant conflict-stop diagnostic helpers are sampled in the Tactix hot loop (kernel_pose_trace alone 1.31%); neither 5382 cost nor this profile isolates production overhead. Follow up with clean Release profiles 5385. No final verdict or timing acceptance.'))

c = root / 'benchmarks/runs/20260907-5385-release-profile'
files = sorted((c / 'results').glob('*.json'))
if len(files) != 6:
    print('5385 pending', len(files), 'of 6'); raise SystemExit(0)
m = json.loads((c / 'manifest.json').read_text())
for rel, expected in m['pin_sha256'].items(): assert sha(c / rel) == expected
old = {}
for p in (root / m['parent_campaign'] / 'results').glob('*.json'):
    r = json.loads(p.read_text()); old[r['rank'], r['arm']] = r
arms = []
for p in files:
    r = json.loads(p.read_text())
    assert r['valid'] and r['expected_match'] and r['verdict'] == 'SAT'
    assert r['artifact_deleted'] and r['derived_artifacts_deleted'] and not r['cleanup_pending']
    assert r['solve']['exit_code'] == 10 and not r['solve']['timed_out']
    assert r['model_check']['exit_code'] == 0 and not r['model_check']['timed_out']
    assert r['binary_sha256'] == m['arms'][r['arm']]['sha256']
    work = counters(r['solver_statistics'])
    assert work == counters(old[r['rank'], r['arm']]['solver_statistics'])
    profile = r['solve']['profile']; stem = f"{r['task']:03d}"
    for n, report in profile['reports'].items():
        assert report['exit'] == 0 and report['sha256'] == sha(c / 'profiles' / (stem + '-' + n + '.txt'))
    assert sha(c / 'profiles' / (stem + '.perf.data.xz')) == profile['compressed_sha256']
    samples = self_profile(c / 'profiles' / (stem + '-self.txt'))
    assert not any('kernel_pose_trace' in s['symbol'] for s in samples)
    arms.append(dict(rank=r['rank'], arm=r['arm'], counters=work, self_samples=samples))
save(c, dict(final_records=6, arms=arms, result_sha256={p.name: sha(p) for p in files},
             conclusion='All six clean Release full solves reproduce their unprofiled 5379 work counters and pass independent SAT model checks with clean scratch and usable profiles. Cross-solver trajectories differ; sampling localizes hotspots but is not acceptance timing.'))

#!/usr/bin/env python3
"""Publish clean profile evidence without waiting on pending proof checkers."""
import datetime
import hashlib
import json
from pathlib import Path
import re
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from runtime_summary import counters
c = root / 'benchmarks/runs/20260907-5425-rephase-root-profile'
baseline = root / 'benchmarks/runs/20260907-5421-rephase-root-rank35-runtime'
def sha(p):
    h = hashlib.sha256()
    with p.open('rb') as f:
        for block in iter(lambda: f.read(1048576), b''): h.update(block)
    return h.hexdigest()
m = json.loads((c / 'manifest.json').read_text())
records, paths = {}, {}
for kind in ('provisional', 'results'):
    for p in sorted((c / kind).glob('*.json')):
        r = json.loads(p.read_text()); records[r['arm']], paths[r['arm']] = r, p
if len(records) != 2:
    print('5425 profiles pending', len(records), 'of 2'); raise SystemExit(0)
for rel, digest in m['pin_sha256'].items(): assert sha(c / rel) == digest, rel
old = {}
for kind in ('provisional', 'results'):
    for p in sorted((baseline / kind).glob('*.json')):
        r = json.loads(p.read_text()); old[r['task']] = r
assert len(old) == 9
arms = {}
for arm, r in records.items():
    assert r['expected_match'] and r['verdict'] == 'UNSAT'
    assert r['solve']['exit_code'] == 20 and not r['solve']['timed_out']
    if paths[arm].parent.name == 'results':
        assert r['valid'] and r['artifact_deleted'] and r['derived_artifacts_deleted']
        assert not r['cleanup_pending']
        assert r['proof_check']['exit_code'] == 0 and not r['proof_check']['timed_out']
        assert r['binary_sha256'] == m['arms'][arm]['sha256']
    work = counters(r)
    assert all(v is not None for v in work.values())
    parents = [v for v in old.values() if v['arm'] == arm]
    assert len(parents) == 3 and all(work == counters(v) for v in parents)
    profile = r['solve']['profile']; stem = f"{r['task']:03d}"
    for name, report in profile['reports'].items():
        assert report['exit'] == 0 and report['sha256'] == sha(c / 'profiles' / (stem + '-' + name + '.txt'))
    assert sha(c / 'profiles' / (stem + '.perf.data.xz')) == profile['compressed_sha256']
    body = (c / 'profiles' / (stem + '-self.txt')).read_text()
    assert '# Total Lost Samples: 0' in body
    samples = []
    for line in body.splitlines():
        found = re.match(r'\s*([\d.]+)%\s+(\S+)\s+\[\.\]\s+(.+?)\s+-\s+-\s*$', line)
        if found: samples.append(dict(percent=float(found[1]), dso=found[2], symbol=found[3]))
    assert samples and any(v['dso'] == arm for v in samples)
    assert not any('kernel_pose_trace' in v['symbol'] for v in samples)
    arms[arm] = dict(work=work, self_samples=samples, profile=profile,
        verification='checked' if r.get('valid') else 'pending',
        record=str(paths[arm].relative_to(root)), record_sha256=sha(paths[arm]),
        solver_timing_statistics=[line for line in r['solver_statistics'] if
            line.startswith('c timing') or re.match(r'^c\s+\d+\.\d+\s+\d+\.\d+\s+%\s+', line)])
result = dict(observed_at=datetime.datetime.now(datetime.timezone.utc).isoformat(),
    solver_results=2, checked_clean=sum(r.get('valid', False) for r in records.values()),
    manifest_sha256=sha(c / 'manifest.json'), arms=arms,
    conclusion='Both clean full-solve profiles reproduce all three unprofiled work-counter replicas per arm with zero lost samples. Tactix inprocessing dominates the remaining cost investigation. Differing cross-solver work and timer scopes prevent per-operation cost or matched timing claims. Pending proofs are provisional passes only.')
p = c / ('summary.json' if result['checked_clean'] == 2 else 'solver-checkpoint.json')
if p.exists():
    print('Frozen profile checkpoint already exists:', p); raise SystemExit(0)
p.write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps(dict(solver_results=2, checked_clean=result['checked_clean'],
    arms={a: dict(work=v['work'], top_self=v['self_samples'][:5]) for a, v in arms.items()}), indent=2))

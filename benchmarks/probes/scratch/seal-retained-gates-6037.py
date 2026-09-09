#!/usr/bin/env python3
"""Seal completed5962 certificates without rewriting solver-time snapshots."""
from collections import Counter, defaultdict
import datetime as dt
import hashlib
import json
from pathlib import Path
root = Path(__file__).resolve().parents[3]
target = root / 'benchmarks/reports/2026-09-08-retained5962-checked-gates.json'
assert not target.exists()
hashes = {}
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def retain(p): hashes[str(p.relative_to(root))] = sha(p)
def checked(r):
    assert r['valid'] and r['expected_match'] and r['verdict'] == r['expected']
    assert r['artifact_deleted'] and r['derived_artifacts_deleted'] and not r['cleanup_pending']
    assert not r['solve']['timed_out'] and r['solve']['exit_code'] == (10 if r['expected'] == 'SAT' else 20)
    oracle = r['model_check'] if r['expected'] == 'SAT' else r['proof_check']
    assert oracle['result'] == 'verified' and oracle['exit_code'] == 0 and not oracle['timed_out']
totals = {}; outcomes = {}
for number, count in ((5968, 66), (5970, 40)):
    c = next((root / 'benchmarks/runs').glob(f'20260908-{number}-*'))
    m = json.loads((c / 'manifest.json').read_text()); retain(c / 'manifest.json')
    for rel, digest in m['pin_sha256'].items(): assert sha(c / rel) == digest, rel
    paths = sorted((c / 'results').glob('*.json')); assert len(paths) == count
    arm_times = defaultdict(float); verdicts = Counter()
    for p in paths:
        r = json.loads(p.read_text()); checked(r); retain(p)
        assert r['binary_sha256'] == m['arms'][r['arm']]['sha256']
        provisional = json.loads((c / 'provisional' / p.name).read_text())
        assert r['solve'] == provisional['solve']
        assert all(r[k] == provisional[k] for k in ('instance','expected','certificate_sha256','certificate_bytes'))
        arm_times[r['arm']] += r['solve']['wall_seconds']; verdicts[r['expected']] += 1
    snapshot = c / 'solver-checkpoint.json'; retain(snapshot)
    totals[number] = dict(arm_times); outcomes[number] = dict(checked_clean=count, verdicts=dict(verdicts))
reference = root / 'benchmarks/baselines/rank1-40-kissat-12cpu-v1/manifest.json'
ref = json.loads(reference.read_text()); retain(reference)
assert ref['instances'] == 40 and ref['solver_seconds'] == 6810.066
for row in ref['records']:
    binding = row['measurement_authority']; p = root / binding['path']
    assert sha(p) == binding['sha256']
    final = root / binding['path'].replace('/provisional/', '/results/')
    if row['final_authority']:
        assert str(final.relative_to(root)) == row['final_authority']['path']
        assert sha(final) == row['final_authority']['sha256']
    r = json.loads(final.read_text()); checked(r); retain(final)
    assert r['arm'] == 'kissat' and r['instance'] == row['instance']
    assert r['solve'] == row['measurement']['solve']
    assert r['certificate_sha256'] == row['measurement']['certificate_sha256']
full = sum(totals[5970].values()); ratio = full / ref['solver_seconds']
bc = totals[5968]
assert abs(full - 7603.117) < 1e-8
out = dict(observed_at=dt.datetime.now(dt.timezone.utc).isoformat(), campaigns=outcomes,
    full_seconds=full, fixed_reference_seconds=ref['solver_seconds'], ratio=ratio,
    fixed_reference_checked=40, runtime_target_met=ratio <= 1.01,
    BC_totals=bc, BC_ratios={tier: bc['candidate_'+tier] / bc['current_'+tier] for tier in ('B','C')},
    hashes=hashes,
    limitations='All146 fixed-reference/retained-gate certificates checked and clean. No new timing observations, solver-time snapshot edits, production promotion or current-source fuzz claim. Runtime objective remains unmet.')
target.write_text(json.dumps(out, indent=2) + '\n')
print(json.dumps({k:v for k,v in out.items() if k != 'hashes'}, indent=2))

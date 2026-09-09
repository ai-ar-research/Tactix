#!/usr/bin/env python3
"""Frozen three-rank paired-wave checkpoint; pending certificates stay pending."""
import csv
import datetime as dt
import json
from pathlib import Path
import statistics
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from checked_comparison import classify
from fixed_runtime import sha

c = root / 'benchmarks/runs/20260909-6332-collapse-three-wave-runtime'
target = c / 'solver-checkpoint-6343.json'
assert not target.exists(), 'frozen checkpoint exists'
m = json.loads((c / 'manifest.json').read_text())
bindings = {str((c / 'manifest.json').relative_to(root)): sha(c / 'manifest.json')}
for rel, digest in m['pin_sha256'].items():
    assert sha(c / rel) == digest, rel
for rel, digest in m['protected_campaign_hashes'].items():
    assert sha(root / rel) == digest, rel
for binding in m['authorities'].values():
    assert sha(root / binding['path']) == binding['sha256']
with (c / 'matrix.tsv').open() as stream:
    matrix = list(csv.DictReader(stream, delimiter='\t'))
assert [int(r['task']) for r in matrix] == list(range(18))
for wave in range(3):
    q = c / f'wave{wave}-submission.json'
    launch = json.loads(q.read_text())
    assert launch['tasks'] == list(range(6*wave, 6*wave+6))
    assert launch['manifest_sha256'] == sha(c / 'manifest.json')
    bindings[str(q.relative_to(root))] = sha(q)
    group = matrix[6*wave:6*wave+6]
    assert {(int(r['rank']), r['arm']) for r in group} == {
        (rank, arm) for rank in (32, 33, 40) for arm in ('candidate', 'previous')}

rows = {}; statuses = {}
for folder in ('provisional', 'results'):
    for q in sorted((c / folder).glob('*.json')):
        r = json.loads(q.read_text()); task = r['task']
        assert task == int(q.stem) and task in range(18)
        state = classify(r, folder == 'results')
        assert state['status'] in ('pending_validation', 'checked_UNSAT'), (q, state)
        for key in ('arm', 'instance', 'expected'):
            assert r[key] == matrix[task][key], (q, key)
        assert r['rank'] == int(matrix[task]['rank'])
        if folder == 'results':
            assert r['binary_sha256'] == m['arms'][r['arm']]['sha256']
        binding = m['work_oracles'][r['arm']][str(r['rank'])]
        assert sha(root / binding['path']) == binding['sha256']
        oracle = json.loads((root / binding['path']).read_text())
        for key in ('certificate_sha256', 'certificate_bytes', 'verdict', 'instance'):
            assert r[key] == oracle[key], (q, key)
        nt = lambda v: [s for s in v['solver_statistics'] if not s.startswith('c timing')]
        assert nt(r) == nt(oracle), q
        if task in rows:
            for key in ('solve', 'certificate_sha256', 'certificate_bytes'):
                assert r[key] == rows[task][key], (q, key)
        rows[task], statuses[task] = r, state
        bindings[str(q.relative_to(root))] = sha(q)
assert set(rows) == set(range(18)), 'all three complete solver waves required'

ranks = {}
for rank in (32, 33, 40):
    waves = []
    for wave in range(3):
        arms = {r['arm']: r for t, r in rows.items() if t // 6 == wave and r['rank'] == rank}
        assert set(arms) == {'candidate', 'previous'}
        times = {a: r['solve']['wall_seconds'] for a, r in arms.items()}
        assert min(times.values()) > 0
        waves.append(dict(wave=wave, seconds=times, ratio=times['candidate']/times['previous'],
            placement={a: dict(node=r['node'], affinity=r.get('cpu_affinity'),
                peak_rss_kib=r['solve']['peak_rss_kb'], runtime_context=r.get('runtime_context'))
                for a, r in arms.items()}))
    spread = {}
    for arm in ('candidate', 'previous'):
        values = [w['seconds'][arm] for w in waves]
        median = statistics.median(values)
        spread[arm] = dict(median=median, min=min(values), max=max(values),
                           range_over_median=(max(values)-min(values))/median)
    ratios = [w['ratio'] for w in waves]
    ranks[rank] = dict(waves=waves, spread=spread,
        paired=dict(median=statistics.median(ratios), min=min(ratios), max=max(ratios)),
        absolute_range_within_two_percent=all(v['range_over_median'] <= .02 for v in spread.values()))
result = dict(observed_at=dt.datetime.now(dt.timezone.utc).isoformat(), solver_outcomes=18,
    checked_clean=sum(v['checked'] for v in statuses.values()), statuses=statuses,
    exact_per_arm_work_and_proof=True, ranks=ranks, hashes=bindings,
    limitation='Changed-work candidate6305 versus6269, not isolated unit cost or fixed50 competitiveness. Pending proofs do not close a checked gate; source fuzz and memory remain open. Missing affinity/context in provisional v1 is unknown, not inferred.')
target.write_text(json.dumps(result, indent=2)+'\n')
print(json.dumps({k: v for k, v in result.items() if k not in ('hashes', 'statuses')}, indent=2))

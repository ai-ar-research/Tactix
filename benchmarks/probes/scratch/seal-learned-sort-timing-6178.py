#!/usr/bin/env python3
"""Summarize all three exact-work pairs; certificate checking stays separate."""
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
c = root / 'benchmarks/runs/20260909-6176-learned-sort-rank4-runtime'
m = json.loads((c / 'manifest.json').read_text())
for rel, digest in m['pin_sha256'].items():
    assert sha(c / rel) == digest, rel
with (c / 'matrix.tsv').open(newline='') as stream:
    matrix = list(csv.DictReader(stream, delimiter='\t'))
old_path = root / m['predecessor_rank4_record']['path']
assert sha(old_path) == m['predecessor_rank4_record']['sha256']
old = json.loads(old_path.read_text())
rows = {}; bindings = {}; statuses = {}
for folder in ('provisional', 'results'):
    for p in sorted((c / folder).glob('*.json')):
        r = json.loads(p.read_text()); task = r['task']
        status = classify(r, folder == 'results')
        assert status['status'] in ('pending_validation', 'checked_UNSAT'), (p, status)
        for key in ('arm', 'instance', 'expected'):
            assert r[key] == matrix[task][key], (p, key)
        if folder == 'results':
            assert r['binary_sha256'] == m['arms'][r['arm']]['sha256']
        # Provisional v1 omits command, binary digest and affinity. The runner
        # pin is verified above; final records must supply those identities.
        for key in ('certificate_sha256', 'certificate_bytes', 'verdict', 'instance'):
            assert r[key] == old[key], (p, key)
        nt = lambda v: [s for s in v['solver_statistics'] if not s.startswith('c timing')]
        assert nt(r) == nt(old), p
        if task in rows:
            assert r['solve'] == rows[task]['solve'], p
        rows[task], statuses[task] = r, status
        bindings[str(p.relative_to(root))] = sha(p)
assert set(rows) == set(range(6)), 'all three waves required'
waves = []
for wave in range(3):
    arms = {rows[t]['arm']: rows[t] for t in (2*wave, 2*wave+1)}
    t, p = arms['candidate'], arms['previous']
    waves.append(dict(wave=wave, candidate_seconds=t['solve']['wall_seconds'],
        previous_seconds=p['solve']['wall_seconds'], ratio=t['solve']['wall_seconds']/p['solve']['wall_seconds'],
        placement={a: dict(node=r['node'], affinity=r.get('cpu_affinity'),
                          peak_rss_kib=r['solve']['peak_rss_kb']) for a, r in arms.items()}))
ratios = [w['ratio'] for w in waves]
spread = {}
for arm in ('candidate', 'previous'):
    values = [r['solve']['wall_seconds'] for r in rows.values() if r['arm'] == arm]
    median = statistics.median(values)
    spread[arm] = dict(median=median, min=min(values), max=max(values), range_over_median=(max(values)-min(values))/median)
checked = sum(s['checked'] for s in statuses.values())
target = c / ('summary.json' if checked == 6 else 'solver-checkpoint.json')
assert not target.exists(), 'frozen checkpoint exists'
result = dict(observed_at=dt.datetime.now(dt.timezone.utc).isoformat(), manifest_sha256=sha(c / 'manifest.json'),
    solver_results=6, checked_clean=checked, exact_predecessor_work_and_proof=True,
    waves=waves, paired=dict(median=statistics.median(ratios), min=min(ratios), max=max(ratios)),
    spread=spread, absolute_range_within_two_percent=all(s['range_over_median'] <= .02 for s in spread.values()),
    hashes=bindings, limitation='Matched exact-work local unit-cost evidence, not global competitiveness. Pending checks do not establish a checked gate; full50, memory and source-fuzz gates remain open.')
target.write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps({k: result[k] for k in ('checked_clean', 'waves', 'paired', 'spread', 'absolute_range_within_two_percent')}, indent=2))

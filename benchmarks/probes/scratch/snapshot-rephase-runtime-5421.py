#!/usr/bin/env python3
"""Freeze all three matched solver replicas, preserving pending-check status."""
import collections
import datetime
import hashlib
import json
from pathlib import Path
import statistics
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from runtime_summary import counters

c = root / 'benchmarks/runs/20260907-5421-rephase-root-rank35-runtime'
m = json.loads((c / 'manifest.json').read_text())
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
for rel, digest in m['pin_sha256'].items(): assert sha(c / rel) == digest, rel
rows, paths = {}, {}
for kind in ('provisional', 'results'):
    for p in sorted((c / kind).glob('*.json')):
        r = json.loads(p.read_text())
        assert r['expected_match'] and r['solve']['exit_code'] == 20 and not r['solve']['timed_out']
        if kind == 'results':
            assert r['binary_sha256'] == m['arms'][r['arm']]['sha256']
            assert r['valid'] and r['artifact_deleted'] and r['derived_artifacts_deleted'] and not r['cleanup_pending']
            assert r['proof_check']['exit_code'] == 0 and not r['proof_check']['timed_out']
        rows[r['task']], paths[r['task']] = r, p
if len(rows) != 9:
    print('5421 solver results pending:', len(rows), 'of 9')
    raise SystemExit(0)
assert set(rows) == set(range(9))
waves, groups = [], collections.defaultdict(list)
for wave in range(3):
    arms = {rows[i]['arm']: rows[i] for i in m['wave_tasks'][str(wave)]}
    assert set(arms) == {'candidate', 'previous', 'kissat'}
    seconds = {arm: r['solve']['wall_seconds'] for arm, r in arms.items()}
    waves.append(dict(wave=wave, seconds=seconds,
        candidate_over_previous=seconds['candidate']/seconds['previous'],
        candidate_over_kissat=seconds['candidate']/seconds['kissat']))
for i, r in sorted(rows.items()):
    groups[r['arm']].append(dict(task=i, seconds=r['solve']['wall_seconds'],
        peak_rss_kb=r['solve']['peak_rss_kb'], work=counters(r), node=r['node'],
        cpu_affinity=r.get('cpu_affinity'), verification='checked' if r.get('valid') else 'pending',
        record=str(paths[i].relative_to(root)), record_sha256=sha(paths[i])))
spread = {}
for arm, rs in groups.items():
    times = [r['seconds'] for r in rs]
    spread[arm] = dict(median=statistics.median(times), min=min(times), max=max(times),
                      range_over_median=(max(times)-min(times))/statistics.median(times))
paired = {}
for key in ('candidate_over_previous', 'candidate_over_kissat'):
    values = [w[key] for w in waves]
    paired[key] = dict(median=statistics.median(values), min=min(values), max=max(values))
result = dict(observed_at=datetime.datetime.now(datetime.timezone.utc).isoformat(),
    campaign=str(c.relative_to(root)), manifest_sha256=sha(c / 'manifest.json'),
    solver_results=9, checked_clean=sum(r.get('valid', False) for r in rows.values()),
    expected_unsat=9, timeouts=0, waves=waves, arms=dict(groups), spread=spread, paired=paired,
    absolute_range_within_two_percent=all(s['range_over_median'] <= .02 for s in spread.values()),
    limitations='Solver timing checkpoint; pending proofs are provisional passes for investigation only. No production promotion or global goal closure. Independent full-width replica blocks; node placement is recorded, not forced or crossed. Affinity/runtime context and per-record binary identity publish with final records; immutable pin hashes are independently verified here.')
dest = c / 'solver-checkpoint.json'
assert not dest.exists(), 'Frozen checkpoint already exists; do not overwrite it'
dest.write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps({k: result[k] for k in ('solver_results', 'checked_clean', 'spread', 'paired', 'absolute_range_within_two_percent')}, indent=2))

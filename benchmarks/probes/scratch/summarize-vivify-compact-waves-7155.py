#!/usr/bin/env python3
"""Validate all own-work repeats and report all three paired runtime waves."""
import csv
import importlib.util
import json
from pathlib import Path
import statistics
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
from inprocessing_cost import paired_costs
reader = Path(__file__).with_name('submit-vivify-compact-wave-7150.py')
spec = importlib.util.spec_from_file_location('collect_repeat', reader)
h = importlib.util.module_from_spec(spec); spec.loader.exec_module(h)
h.fixtures()
c = root / 'benchmarks/runs/20260910-7150-vivify-compact-runtime'
target = c / 'solver-checkpoint-7155.json'
assert not target.exists()
hashes = {}
def bind(p, expected=None):
    digest = sha(p)
    assert expected is None or expected == digest, p
    hashes[str(p.relative_to(root))] = digest
m = json.loads((c / 'manifest.json').read_text()); bind(c / 'manifest.json')
for rel, digest in m['pin_sha256'].items(): bind(c / rel, digest)
for group in ('dependencies', 'protected_campaign_hashes'):
    for rel, digest in m[group].items(): bind(root / rel, digest)
for binding in m['authorities'].values(): bind(root / binding['path'], binding['sha256'])
with (c / 'matrix.tsv').open() as stream: matrix = list(csv.DictReader(stream, delimiter='\t'))
assert [int(r['task']) for r in matrix] == list(range(18))
launched = set()
for wave in range(3):
    path = c / f'wave{wave}-submission.json'
    if not path.exists(): continue
    launch = json.loads(path.read_text()); bind(path)
    assert launch['manifest_sha256'] == sha(c / 'manifest.json')
    assert launch['tasks'] == list(range(6*wave, 6*wave+6))
    assert {(int(r['rank']), r['arm']) for r in matrix[6*wave:6*wave+6]} == {
        (rank, arm) for rank in (31, 35, 38) for arm in ('candidate', 'previous')}
    launched.update(launch['tasks'])
rows, states = {}, {}
for folder in ('provisional', 'results'):
    for path in sorted((c / folder).glob('*.json')):
        r = json.loads(path.read_text()); task = r['task']; bind(path)
        assert task == int(path.stem) and task in launched
        state = classify(r, folder == 'results')
        assert state['status'] in ('pending_validation', 'checked_UNSAT'), (path, state)
        for key in ('arm', 'instance', 'expected'): assert r[key] == matrix[task][key], (path, key)
        assert r['rank'] == int(matrix[task]['rank'])
        if folder == 'results': assert r['binary_sha256'] == m['arms'][r['arm']]['sha256']
        if task in rows:
            assert h.fingerprint(r) == h.fingerprint(rows[task]) and r['solve'] == rows[task]['solve']
        rows[task], states[task] = r, state
first = {(r['arm'], r['rank']): r for task, r in rows.items() if task < 6}
for task, r in rows.items():
    assert h.fingerprint(r) == h.fingerprint(first[(r['arm'], r['rank'])]), (task, 'own repeat changed')
    if r['arm'] in ('previous', 'candidate'):
        binding = m['previous_work_oracles'][str(r['rank'])]
        path = root / binding['path']; bind(path, binding['sha256'])
        assert h.fingerprint(r) == h.fingerprint(json.loads(path.read_text())), task
checked = sum(s['checked'] for s in states.values())
print(f'Expected outcomes {len(rows)}/18; checked/clean {checked}/18; launched {len(launched)}/18')
if set(rows) != set(range(18)):
    print('Incomplete waves; no decision-grade summary written'); sys.exit(0)
ranks = {}
for rank in (31, 35, 38):
    waves = []
    for wave in range(3):
        pair = {r['arm']: r for task, r in rows.items() if task // 6 == wave and r['rank'] == rank}
        assert set(pair) == {'candidate', 'previous'}
        seconds = {arm: r['solve']['wall_seconds'] for arm, r in pair.items()}
        assert min(seconds.values()) > 0
        waves.append(dict(wave=wave, seconds=seconds, ratio=seconds['candidate']/seconds['previous'],
            placement={arm: dict(node=r['node'], affinity=r.get('cpu_affinity'),
                peak_rss_kib=r['solve']['peak_rss_kb'], runtime_context=r.get('runtime_context')) for arm, r in pair.items()}))
    spread = {}
    for arm in ('candidate', 'previous'):
        values = [w['seconds'][arm] for w in waves]; med = statistics.median(values)
        spread[arm] = dict(median=med, min=min(values), max=max(values), range_over_median=(max(values)-min(values))/med)
    ratios = [w['ratio'] for w in waves]
    ranks[rank] = dict(waves=waves, spread=spread,
        paired=dict(median=statistics.median(ratios), min=min(ratios), max=max(ratios)),
        absolute_range_within_two_percent=all(v['range_over_median'] <= .02 for v in spread.values()),
        first_wave_statistics={arm: first[(arm, rank)]['solver_statistics'] for arm in ('candidate', 'previous')})
costs = paired_costs(list(rows.values()), (31, 35, 38), 3, ('Vivify',))
for p in (reader, Path(__file__), root / 'tools/bench/inprocessing_cost.py'): bind(p)
with target.open('x') as stream:
    json.dump(dict(hashes=hashes, solver_outcomes=18, checked_clean=checked,
        exact_own_repeats=True, exact_cross_arm_work=True, ranks=ranks, states=states,
        paired_printed_costs=costs,
        limitation='7144 versus7030 compact canonical Vivify pilot; exact reported work/proofs, not full hidden-state equality or fixed50 competitiveness. Pending proofs are unverified. Complete-Vivify15% target requires separate matched profiles, at least1% whole-runtime median reduction on BOTH31/35 and no runtime/RSS regression31/35/38. No favorable-repeat selection or threshold relaxation.'), stream, indent=2)
    stream.write('\n')
print(json.dumps({rank: value['paired'] for rank, value in ranks.items()}, indent=2))

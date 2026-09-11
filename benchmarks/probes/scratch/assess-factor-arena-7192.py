#!/usr/bin/env python3
"""Assess every declared arena runtime and complete Factor profile repetition."""
import importlib.util
import json
import math
from pathlib import Path
import statistics
import subprocess
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
from inprocessing_cost import paired_costs

def distribution(values):
    assert len(values) == 3 and all(not isinstance(v, bool) and isinstance(v, (int, float)) and math.isfinite(v) and v > 0 for v in values)
    return dict(values=values, median=statistics.median(values), min=min(values), max=max(values))

def fixtures():
    assert distribution([1.1, .9, 1.]) == dict(values=[1.1, .9, 1.], median=1., min=.9, max=1.1)
    for values in ([], [1, 1], [1, 1, 0], [True, 1, 1], [float('nan'), 1, 1]):
        try: distribution(values)
        except AssertionError: pass
        else: raise AssertionError('bad distribution accepted')
    print('Arena assessment: full median/range plus five invalid-distribution controls PASS')

def main():
    assert sys.argv[1:] in ([], ['--self-test'])
    fixtures()
    if sys.argv[1:]: return
    target = root / 'benchmarks/reports/2026-09-11-factor-arena-assessment-7192.json'
    assert not target.exists()
    runtime = root / 'benchmarks/runs/20260910-7186-factor-arena-runtime'
    samples = root / 'benchmarks/runs/20260910-7188-factor-arena-samples'
    if not all((samples / f'wave{wave}/summary.json').exists() for wave in range(3)):
        print('Three complete extracted profile pairs pending; no output created'); return
    subprocess.run([sys.executable, str(Path(__file__).with_name('check-submit-factor-arena-runtime-7186.py'))], check=True)
    source = Path(__file__).with_name('validate-factor-arena-profiles-7187.py')
    spec = importlib.util.spec_from_file_location('profile_gate', source)
    gate = importlib.util.module_from_spec(spec); spec.loader.exec_module(gate)
    profile = gate.read_profiles(); assert len(profile['rows']) == 6
    hashes = dict(profile['hashes'])
    def bind(p, expected=None):
        digest = sha(p); assert expected is None or digest == expected, p
        hashes[str(p.relative_to(root))] = digest
    m = json.loads((runtime / 'manifest.json').read_text()); bind(runtime / 'manifest.json')
    for rel, digest in m['pin_sha256'].items(): bind(runtime / rel, digest)
    for rel, digest in m['dependencies'].items(): bind(root / rel, digest)
    rows, states = [], []
    for task in range(18):
        p = runtime / f'results/{task:03d}.json'; final = p.exists()
        if not final: p = runtime / f'provisional/{task:03d}.json'
        if not p.exists():
            print('Three complete runtime waves pending; no output created'); return
        r = json.loads(p.read_text()); state = classify(r, final)
        assert state['status'] in ('pending_validation', 'checked_UNSAT')
        bind(p); rows.append(r); states.append(state)
    costs = paired_costs(rows, [31, 35, 38], 3, ['Factor', 'Vivify', 'BVE'])
    spreads = {}
    for rank in (31, 35, 38):
        arms = {arm: [r['solve']['wall_seconds'] for r in rows if r['rank'] == rank and r['arm'] == arm] for arm in ('candidate', 'previous')}
        values = {arm: max(series)/min(series)-1 for arm, series in arms.items()}
        spreads[rank] = dict(by_arm=values, absolute_headline_stable=max(values.values()) <= .02)
    waves, factor_ratios = [], []
    for wave in range(3):
        p = samples / f'wave{wave}/summary.json'; bind(p); r = json.loads(p.read_text())
        assert r['wave'] == wave
        for rel, digest in r['hashes'].items(): bind(root / rel, digest)
        for arm in ('candidate', 'previous'):
            assert r['function_envelopes'][arm]['total_period'] == r['arms'][arm]['total_period']
        ratio = r['function_envelopes']['candidate']['selected_period'] / r['function_envelopes']['previous']['selected_period']
        assert ratio == r['complete_factor_ratio']
        factor_ratios.append(ratio)
        waves.append(dict(wave=wave, envelopes=r['function_envelopes'], phase_ratios=r['phase_ratios']))
        bind(runtime / f'wave{wave}-submission.json')
    bind(Path(__file__)); bind(root / 'tools/bench/inprocessing_cost.py')
    result = dict(hashes=hashes, runtime=costs, spreads=spreads, complete_factor=distribution(factor_ratios),
        profiles=waves, runtime_expected=18, profile_expected=6,
        runtime_checked_clean=sum(s['checked'] for s in states),
        profiles_checked_clean=sum(s['checked'] for s in profile['states'].values()),
        interpretation='Every predeclared pair retained. Net runtime, full Factor envelope and unprofiled RSS are separate dimensions. No arbitrary minimum gain. Mixed runtime or increased memory needs an explicit tradeoff decision, not a hidden exclusion. Code displacement and environment can offset a real local cost reduction.',
        limitation='No final-check, fuzz, production integration, memory parity or all50 transfer acceptance from these targeted results. The fixed Kissat denominator and established closures are unchanged. Pending proofs remain pending.',
        next='Decide whether the measured net gain supports retaining the isolated prototype and a fresh all50 transfer. Keep larger Vivify/Sweep/Congruence and31/43 search-work gaps in priority; do not keep iterating tiny Factor changes by default.')
    with target.open('x') as out: json.dump(result, out, indent=2)
    print(target); print('Complete Factor', result['complete_factor'])
    for rank, data in costs.items(): print(rank, data['paired'], spreads[rank])

if __name__ == '__main__': main()

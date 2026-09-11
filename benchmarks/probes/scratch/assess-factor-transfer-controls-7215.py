#!/usr/bin/env python3
"""Retain all predeclared transfer repeats, including unstable adverse waves."""
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
from inprocessing_cost import paired_costs, read_timers


def main():
    c = root / 'benchmarks/runs/20260911-7212-factor-arena-transfer-controls'
    target = root / 'benchmarks/reports/2026-09-11-factor-transfer-assessment-7215.json'
    assert not target.exists(), 'successful writer frozen'
    subprocess.run([sys.executable, str(Path(__file__).with_name('check-submit-factor-arena-transfer-controls-7212.py'))], check=True)
    reader = Path(__file__).with_name('summarize-factor-transfer-7213.py')
    spec = importlib.util.spec_from_file_location('counter_context', reader)
    h = importlib.util.module_from_spec(spec); spec.loader.exec_module(h); h.fixtures()
    hashes = {}
    def bind(p, expected=None):
        digest = sha(p); assert expected is None or digest == expected, p
        hashes[str(p.relative_to(root))] = digest
    m = json.loads((c / 'manifest.json').read_text()); bind(c / 'manifest.json')
    for rel, digest in m['pin_sha256'].items(): bind(c / rel, digest)
    for rel, digest in m['dependencies'].items(): bind(root / rel, digest)
    rows, states, context = [], [], []
    for task in range(18):
        p = c / f'results/{task:03d}.json'; final = p.exists()
        if not final: p = c / f'provisional/{task:03d}.json'
        if not p.exists():
            print('Complete three waves pending; no output created'); return
        r = json.loads(p.read_text()); state = classify(r, final)
        assert state['status'] in ('pending_validation', 'checked_SAT'), (p, state)
        bind(p); rows.append(r); states.append(state)
        timers = read_timers(r['solver_statistics'])
        assert 'Factor' not in timers['rules']
        assert set(timers['rules']) == {'Congruence', 'Substitute', 'Sweep', 'Backbone'}
        assert all(v['applications'] == 1 for v in timers['rules'].values())
        context.append(dict(task=task, rank=r['rank'], arm=r['arm'], node=r['node'],
            solve=r['solve'], timers=timers,
            counters=h.perf(r['runtime_context']['perf_statistics']),
            topology=r['runtime_context']['topology']))
    costs = paired_costs(rows, [8, 10, 13], 3, ['Congruence', 'Substitute', 'Sweep'])
    spreads = {}
    for rank in (8, 10, 13):
        by_arm = {}
        for arm in ('candidate', 'previous'):
            times = [r['solve']['wall_seconds'] for r in rows if r['rank'] == rank and r['arm'] == arm]
            by_arm[arm] = max(times)/min(times)-1
        spreads[rank] = dict(by_arm=by_arm, absolute_headline_stable=max(by_arm.values()) <= .02)
    for wave in range(3): bind(c / f'wave{wave}-submission.json')
    for p in (Path(__file__), reader, root / 'tools/bench/inprocessing_cost.py',
              root / '.claude/worktrees/runtime-factor-arena-7180/src/main.cpp',
              root / '.claude/worktrees/runtime-factor-arena-7180/src/rules/factor/factor.cpp',
              root / '.claude/worktrees/runtime-factor-arena-7180/src/policies/default_policy.cpp'):
        bind(p)
    result = dict(hashes=hashes, runtime=costs, spreads=spreads, context=context,
        expected=18, checked_clean=sum(s['checked'] for s in states),
        interpretation='All three candidate spreads exceed2%; preserve every paired median/range but no stable absolute headline. All18 runs report exactly one Congruence/Substitute/Sweep/Backbone application and no Factor application. Source puts the arena construction only inside Factor apply, not guard/enumeration. Direct Factor arena construction cannot cause these three regressions; code-layout and system effects remain possible.',
        decision='Keep full7201 score and RSS tradeoffs unchanged. Do not modify Factor allocations to explain non-executed work, and do not repeat until a favorable stable wave appears. Continue independent Congruence cost and larger Reduce/collection and Rank31 work-quality discriminators.',
        limitation='Counters include kernel execution and fault counts are correlates, not an identified cause. No all50 gain, integration, fuzz, memory-parity or final-proof acceptance. Retained7144 and fixedKissat unchanged.')
    with target.open('x') as out:
        json.dump(result, out, indent=2); out.write('\n')
    print(target)
    for rank, value in costs.items(): print(rank, value['paired']['wall_seconds'], spreads[rank])


if __name__ == '__main__': main()

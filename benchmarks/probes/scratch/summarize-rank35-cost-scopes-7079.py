#!/usr/bin/env python3
"""Compare frozen cycle profiles at explicitly named, non-nested scopes."""
import json
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from checked_comparison import classify
from fixed_runtime import sha
from perf_symbol_cost import read_report, select

target = root / 'benchmarks/reports/2026-09-10-rank35-cost-scopes-7079.json'
assert not target.exists(), 'successful evidence is frozen'
hashes = {}
def bind(path, expected=None):
    digest = sha(path)
    assert expected is None or digest == expected, path
    hashes[str(path.relative_to(root))] = digest

profiles = {}; states = {}
for arm, directory, task in (
    ('candidate', '20260910-7060-rank35-residual-profile', 0),
    ('kissat', '20260910-7002-largest-gap-release-profiles', 3)):
    c = root / 'benchmarks/runs' / directory
    rp = c / f'results/{task:03d}.json'
    bind(rp); record = json.loads(rp.read_text())
    state = classify(record, True)
    assert state['checked'] and state['clean'] and state['status'] == 'checked_UNSAT'
    assert record['rank'] == 35 and record['arm'] == arm
    states[arm] = dict(path=str(rp.relative_to(root)), state=state)
    profile = record['solve']['profile']; parsed = {}
    for name, inclusive in (('self', False), ('callpaths', True)):
        p = c / f'profiles/{task:03d}-{name}.txt'
        assert profile['reports'][name]['exit'] == 0
        bind(p, profile['reports'][name]['sha256'])
        with p.open() as stream: parsed[name] = read_report(stream, inclusive)
    assert parsed['self']['total_cycles'] == parsed['callpaths']['total_cycles']
    runner = c / 'pins/run.py'; bind(runner)
    assert "'--percent-limit', '0.1'" in runner.read_text()
    # Self tables omit symbols below the frozen 0.1% report threshold, while
    # inclusive tables retain their callers. Do not impute missing self costs.
    for key, row in parsed['callpaths']['rows'].items():
        if key not in parsed['self']['rows']:
            assert row['self_percent'] <= 0.1, key
        else:
            assert abs(parsed['self']['rows'][key]['self_percent'] - row['self_percent']) <= 0.010001, key
    profiles[arm] = parsed['callpaths']

def pair(candidate, native, note):
    c = select(profiles['candidate'], 'candidate', candidate)
    n = select(profiles['kissat'], 'kissat', native)
    return dict(candidate=c, kissat=n,
                estimated_excess_cycles=c['estimated_cycles']-n['estimated_cycles'],
                rounding_cycles=c['rounding_cycles']+n['rounding_cycles'], scope=note)

rows = {
    'inprocessing_envelope': pair(
        ['tactix::DefaultInprocessScheduler::run_phase(tactix::Kernel&, tactix::SlotGroup, int volatile*)'],
        ['kissat_probe', 'kissat_probe_initially', 'kissat_eliminate'],
        'Whole function call scopes include root preparation on both sides, unlike their printed timers. Native initial Probe is included explicitly. Not proof of identical semantic work.'),
    'main_propagation': pair(
        ['tactix::kernel_propagate(tactix::Kernel&, unsigned int)'], ['kissat_search_propagate'],
        'All calls to ordinary propagation, including phase-entry/resume calls. Overlaps the inprocessing envelope; do not sum.'),
    'reduce': pair(
        ['tactix::reduce_apply_refs(tactix::Kernel&, unsigned int const*, unsigned long, bool)'], ['kissat_reduce'],
        'Reduction and its collection children. General API versus native wrapper scopes, not exact instruction or memory-work equality.'),
    'vivify': pair(['tactix::vivify_apply(tactix::Kernel&)'], ['kissat_vivify'],
        'Scheduling, private propagation and watch transitions; nested within the inprocessing envelope.'),
    'factor': pair(['tactix::factor_apply_seeded(tactix::Kernel&, unsigned int)'], ['kissat_factor'],
        'Factor entry through return; native may include guard-only calls; nested within inprocessing.'),
    'congruence': pair(['tactix::congruence_apply(tactix::Kernel&)'], ['kissat_congruence'],
        'Congruence entry through return, including initial native calls; gate extraction counts differ, so not a unit-cost claim.'),
    'bve_attempts': pair(
        ['tactix::(anonymous namespace)::bve_eliminate(tactix::Kernel&, unsigned int, tactix::(anonymous namespace)::BveDefScratch*)'],
        ['kissat_generate_resolvents'],
        'Diagnostic attempt-function envelopes only: different mutation/guard responsibilities; NOT equivalent BVE prices.'),
    'forward': pair(['tactix::strengthen_apply(tactix::Kernel&)'], ['kissat_forward_subsume_during_elimination'],
        'Forward subsumption/strengthening wrapper scopes; not every dispatch/admission cost is shared.'),
}
for p in (Path(__file__), root / 'tools/bench/perf_symbol_cost.py',
          root / 'tools/bench/test_perf_symbol_cost.py',
          root / 'benchmarks/runs/20260910-7060-rank35-residual-profile/solver-summary-7063.json',
          root / 'benchmarks/runs/20260910-7041-collect-retry-runtime/matched-native-work-7059.json',
          root / 'benchmarks/runs/20260910-7073-rank31-bve-priority/solver-summary-7075.json'):
    bind(p)
for rel in ('src/policies/default_policy.cpp', 'src/kernel/kernel.cpp',
            'src/rules/vivify/vivify.cpp'):
    bind(root / '.claude/worktrees/runtime-collect-retry-7030' / rel)
for rel in ('kissat/src/probe.c', 'kissat/src/eliminate.c', 'kissat/src/vivify.c',
            'cadical/src/vivify.cpp'):
    bind(root / rel)
total_delta = profiles['candidate']['total_cycles'] - profiles['kissat']['total_cycles']
assert total_delta > 0
out = dict(hashes=hashes, states=states,
           total_cycles={a:p['total_cycles'] for a,p in profiles.items()},
           total_excess_cycles=total_delta, scopes=rows,
           inprocessing_share_of_sampled_excess=rows['inprocessing_envelope']['estimated_excess_cycles']/total_delta,
           decision='Prioritize broad inprocessing implementation cost, including schedule construction and maintenance, while auditing trigger/effort/retry strength. Do not optimize main BCP merely because it is hottest. Rank31 initial-admission witness is now localized; no additional raw heap-tie probe selected.',
           limitations='Two already-frozen profile windows, not replicated cost acceptance. Rounded sample-period estimates, not exact hardware event counts or per-operation prices. Rounding bounds exclude sampling, unwinding and hardware uncertainty. Major work agreement is not full-state/effort identity. Nested scope rows must not be added. No solver change, job submission, proof gate change or promotion.')
with target.open('x') as stream:
    json.dump(out, stream, indent=2); stream.write('\n')
print(target)
print('Total sampled excess Gcycles:', total_delta/1e9)
for name, row in rows.items():
    print(name, *(round(row[a]['estimated_cycles']/1e9, 3) for a in ('candidate', 'kissat')),
          'delta', round(row['estimated_excess_cycles']/1e9, 3))
print('Inprocessing envelope share of sampled excess:', out['inprocessing_share_of_sampled_excess'])

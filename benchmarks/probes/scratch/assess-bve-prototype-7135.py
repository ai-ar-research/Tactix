#!/usr/bin/env python3
"""Assess the complete predeclared pilot and rank remaining measured envelopes."""
import json
from pathlib import Path
import statistics
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
from inprocessing_cost import read_timers

target = root / 'benchmarks/reports/2026-09-10-bve-prototype-assessment-7135.json'
assert not target.exists(), 'preserve successful evidence'
hashes = {}
def read(rel, expected=None):
    p = root / rel
    digest = sha(p)
    assert expected is None or digest == expected, rel
    hashes[rel] = digest
    return json.loads(p.read_text())

runtime = read('benchmarks/runs/20260910-7125-bve-import-runtime/solver-checkpoint-7127.json')
assert runtime['solver_outcomes'] == 18 and runtime['exact_cross_arm_work'] and runtime['exact_own_repeats']
profile = read('benchmarks/runs/20260910-7128-bve-lifecycle-profiles/solver-summary-7130.json')
assert profile['solver_outcomes'] == 6 and profile['exact_unprofiled_work_and_proofs']
path = 'benchmarks/runs/20260910-7132-bve-lifecycle-samples/'
phase = read(path + 'three-wave-summary-7133.json')
waves = [read(path + f'wave{i}/summary.json', phase['hashes'][path + f'wave{i}/summary.json']) for i in range(3)]
assert [w['wave'] for w in waves] == list(range(3))
native_path = 'benchmarks/runs/20260910-7081-rank35-callstack-cost/lifecycle-summary-7082.json'
replay = read('benchmarks/reports/2026-09-10-bve-lifecycle-reader-replay-7131.json')
native = read(native_path, replay['hashes'][native_path])['arms']['kissat']
def describe(values):
    return dict(values=values, median=statistics.median(values), min=min(values), max=max(values))

remaining = {}
for name in ('probe', 'eliminate'):
    med = phase['phases'][name]['arms']['candidate']['median']
    remaining[name] = dict(candidate_median_period=med, frozen_native_period=native['phase_periods'][name],
        cross_window_difference=med - native['phase_periods'][name])
components = {}
names = set(native['components']['probe'])
for wave in waves:
    arm = wave['arms']['candidate']
    assert sum(arm['components']['probe'].values()) == arm['phase_periods']['probe']
    assert sum(arm['self_by_phase']['probe'].values()) == arm['phase_periods']['probe']
    names.update(arm['components']['probe'])
for name in sorted(names):
    values = [w['arms']['candidate']['components']['probe'].get(name, 0) for w in waves]
    components[name] = dict(candidate=describe(values), frozen_native=native['components']['probe'].get(name, 0))

# Exclusive self samples are disjoint. Include every instantiated candidate
# introsort, not only the hottest lambda. Do not sum inclusive symbol maps.
schedule = []
for wave in waves:
    rows = {name: value for name, value in wave['arms']['candidate']['self_by_phase']['probe'].items()
        if name.startswith('tactix::vivify_schedule(') or
           name.startswith('void std::__introsort_loop<__gnu_cxx::__normal_iterator<tactix::VivifyCand*')}
    assert len(rows) >= 2
    schedule.append(dict(wave=wave['wave'], symbols=rows, total=sum(rows.values())))

score = read('benchmarks/reports/2026-09-10-collect-retry-checked50-7098/summary.json')
records = [rel for rel in score['hashes']
    if rel.startswith('benchmarks/runs/20260910-7053-collect-retry-ranks1-50/results/')]
assert len(records) == 50
rule_totals, ranks, per_rank = {}, set(), []
for rel in records:
    record = read(rel, score['hashes'][rel])
    status = classify(record, True)
    assert status['checked'] and status['clean'] and record['rank'] not in ranks
    ranks.add(record['rank'])
    timers = read_timers(record['solver_statistics'])
    per_rank.append(dict(rank=record['rank'], timers=timers))
    for name, value in timers['rules'].items():
        rule_totals[name] = rule_totals.get(name, 0) + value['ms']
assert ranks == set(range(1, 51))
gates = dict(eliminate_twenty_percent=phase['eliminate_twenty_percent_sampled_target'],
    no_peak_rss_regression=all(r['paired']['peak_rss_kib']['max'] <= 1 for r in runtime['paired_printed_costs'].values()))
assert gates == dict(eliminate_twenty_percent=False, no_peak_rss_regression=False)
for rel in (str(Path(__file__).relative_to(root)), 'tools/bench/inprocessing_cost.py',
            '.claude/worktrees/runtime-bve-import-bounds-7116/src/rules/vivify/vivify.cpp',
            '.claude/worktrees/runtime-collect-retry-7030/src/rules/vivify/vivify.cpp',
            'kissat/src/vivify.c', 'cadical/src/vivify.cpp'):
    hashes[rel] = sha(root / rel)
out = dict(hashes=hashes, outcome='STOP_AND_REASSESS_UNPROMOTED_PROTOTYPE', original_gates=gates,
    runtime={rank: dict(paired=value['paired'], spread=value['spread'],
                       costs=runtime['paired_printed_costs'][rank]['paired'])
             for rank, value in runtime['ranks'].items()},
    lifecycle=phase['phases'], remaining_phase_envelopes=remaining,
    probe_components=components, vivify_schedule_exclusive=describe([w['total'] for w in schedule]),
    vivify_schedule_symbol_evidence=schedule,
    checked7030_full50_named_rule_ms=dict(sorted(rule_totals.items(), key=lambda item: -item[1])),
    checked7030_per_rank_timers=sorted(per_rank, key=lambda r: r['rank']),
    limitations=[
        'Pending runtime/profile proofs remain unverified. Later published status belongs to the successor audit.',
        'Native7082 is one frozen cross-window diagnostic, not a refreshed timing denominator or exact operation price.',
        'Component wrappers differ across solvers; component differences are leads, not independently removable costs.',
        'No-phase ancestry is not equivalent to CDCL. Inclusive symbol maps are not additive.',
        'Vivify schedule self includes necessary work. Its complete removal is impossible; no speedup is forecast from this envelope.',
        'The checked full50 named-rule timers rank current workload, not excess cost versus native or projected rewrite savings.'],
    next='Retain7116 and all evidence without promotion; seek a bounded new representation design rather than lowering the original BVE target. Preserve direct debugging, pending-proof follow-up and current-source fuzz obligation.')
with target.open('x') as stream:
    json.dump(out, stream, indent=2)
print(target)
print(json.dumps(dict(gates=gates, remaining=remaining, schedule=out['vivify_schedule_exclusive'],
                      full50_named_rule_ms=out['checked7030_full50_named_rule_ms']), indent=2))

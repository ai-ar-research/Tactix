#!/usr/bin/env python3
"""Assess all predeclared runtime/RSS and complete-Vivify gates unchanged."""
import json
from pathlib import Path
import statistics
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
target = root / 'benchmarks/reports/2026-09-10-vivify-compact-assessment-7158.json'
assert not target.exists()
runtime_path = root / 'benchmarks/runs/20260910-7150-vivify-compact-runtime/solver-checkpoint-7155.json'
profile_path = root / 'benchmarks/runs/20260910-7153-vivify-compact-samples/three-wave-summary-7156.json'
if not runtime_path.exists() or not profile_path.exists():
    print('Full three-wave summaries pending; no assessment written'); sys.exit(0)
hashes = {}
def bind(p, expected=None):
    value = sha(p); assert expected is None or value == expected, p
    hashes[str(p.relative_to(root))] = value
def read(p):
    bind(p); return json.loads(p.read_text())
r = read(runtime_path); p = read(profile_path)
for summary in (r, p):
    for rel, digest in summary['hashes'].items(): bind(root / rel, digest)
assert r['solver_outcomes'] == 18 and r['exact_own_repeats'] and r['exact_cross_arm_work']
waves = [read(profile_path.parent / f'wave{i}/summary.json') for i in range(3)]
for i, wave in enumerate(waves):
    assert wave['wave'] == i
    for rel, digest in wave['hashes'].items(): bind(root / rel, digest)
    assert wave['complete_vivify_ratio'] == p['complete_vivify']['ratios'][i]
ranks = {}
for rank in ('31', '35', '38'):
    data = r['ranks'][rank]; cost = r['paired_printed_costs'][rank]
    assert len(data['waves']) == len(cost['waves']) == 3
    ranks[rank] = dict(runtime=data['paired'], spread=data['spread'],
        rss=cost['paired']['peak_rss_kib'], printed_vivify=cost['paired']['Vivify_ms'],
        all_waves=data['waves'], all_cost_pairs=cost['waves'])
gates = dict(
    complete_vivify_35_at_least_15_percent=p['complete_vivify']['paired']['median'] <= .85,
    runtime_31_at_least_1_percent=ranks['31']['runtime']['median'] <= .99,
    runtime_35_at_least_1_percent=ranks['35']['runtime']['median'] <= .99,
    no_runtime_regression=all(ranks[rank]['runtime']['median'] <= 1 for rank in ranks),
    no_rss_regression=all(ranks[rank]['rss']['median'] <= 1 for rank in ranks),
    all_within_arm_ranges_at_most_2_percent=all(r['ranks'][rank]['absolute_range_within_two_percent'] for rank in ranks))
native_path = root / 'benchmarks/runs/20260910-7081-rank35-callstack-cost/lifecycle-summary-7082.json'
native = read(native_path)
components = {}
for name in ('vivify', 'congruence', 'factor', 'sweep', 'substitute', 'backbone', 'transitive_reduce', 'phase_scaffolding_or_inlined_work'):
    values = [wave['arms']['candidate']['components']['probe'].get(name, 0) for wave in waves]
    reference = native['arms']['kissat']['components']['probe'].get(name, 0)
    components[name] = dict(candidate_three_wave_median=statistics.median(values),
        candidate_values=values, frozen_native=reference,
        cross_window_difference=statistics.median(values)-reference)
for rel in ('src/kernel/kernel.cpp', 'src/kernel/clause.hpp', 'src/rules/congruence/congruence.cpp',
            'src/rules/factor/factor.cpp', 'src/rules/vivify/vivify.cpp'):
    bind(root / '.claude/worktrees/runtime-vivify-compact-7144' / rel)
bind(root / 'kissat/src/congruence.c'); bind(Path(__file__))
with target.open('x') as out:
    json.dump(dict(hashes=hashes, gates=gates,
        decision='PERFORMANCE_PREMISE_MET_PROVISIONAL' if all(gates.values()) else 'SHORT_PREMISE_MISSED',
        complete_vivify=p['complete_vivify'], ranks=ranks,
        frozen_native_probe_components=components,
        reference_caveat='Frozen7082 is cross-window and the phase/component responsibilities and gate counts are not proven equal. Differences rank investigation hypotheses; they are not exact operation prices or independently removable overhead.',
        reviewed_leads='Congruence gate extraction/indexing and shared watch lifecycle remain substantial. Current code already implements in-place shrinking and probing specialization; do not propose those as missing. Compare actual gate admission/iteration, materialization, reconnection and work before choosing a larger redesign.',
        rollback='Frozen7030;7144 source/pins/tests/evidence remain preserved, unpromoted. No small scratch/layout tuning or lowered gates to rescue a failed premise.',
        correctness='All18 exact reported work/proof fingerprints. Final independent checking/cleanup remains owned by published records and a successor audit; pending proofs never authorize promotion.',
        next_action='If any premise gate is missed, stop this pilot and diagnose the largest remaining unit-cost/work gap with source-backed mechanism comparisons. Retain useful tests and evidence, preserve full50/fuzz/memory/consolidation gates and established closures; direct Codex work without routine revamp approval pauses.'), out, indent=2)
print(json.dumps(dict(gates=gates, complete_vivify=p['complete_vivify']['paired'],
    ranks={rank: dict(runtime=d['runtime'], rss=d['rss']) for rank, d in ranks.items()}), indent=2))

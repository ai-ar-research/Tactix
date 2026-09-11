#!/usr/bin/env python3
"""Price explicit non-nested scopes from the sealed current Rank31 profiles."""
import datetime as dt
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'tools/bench'))
from fixed_runtime import sha
from perf_symbol_cost import read_report, select
from checked_comparison import classify

C = ROOT / 'benchmarks/runs/20260911-7389-current-gap-profiles'
TARGET = ROOT / 'benchmarks/reports/2026-09-11-current-rank31-cost-7396.json'
SCOPES = {
    'inprocessing': (
        ['tactix::DefaultInprocessScheduler::run_phase(tactix::Kernel&, tactix::SlotGroup, int volatile*)'],
        ['kissat_probe', 'kissat_probe_initially', 'kissat_eliminate']),
    'ordinary_propagation': (
        ['tactix::kernel_propagate(tactix::Kernel&, unsigned int)'], ['kissat_search_propagate']),
    'reduce': (
        ['tactix::reduce_apply_refs(tactix::Kernel&, unsigned int const*, unsigned long, bool)'], ['kissat_reduce']),
    'vivify': (['tactix::vivify_apply(tactix::Kernel&)'], ['kissat_vivify']),
    'factor': (['tactix::factor_apply_seeded(tactix::Kernel&, unsigned int)'], ['kissat_factor']),
    'congruence': (['tactix::congruence_apply(tactix::Kernel&)'], ['kissat_congruence']),
}


def main():
    assert not TARGET.exists(), 'successful evidence is frozen'
    hashes = {}
    def bind(p, expected=None):
        value = sha(p)
        assert expected is None or value == expected, str(p)
        hashes[str(p.relative_to(ROOT))] = value
    sealpath = C / 'solver-summary-7390.json'
    seal = json.loads(sealpath.read_text()); bind(sealpath)
    for rel, value in seal['hashes'].items(): bind(ROOT / rel, value)
    reports = {}; states = {}; work = {}
    for task, arm in enumerate(('candidate', 'kissat')):
        recordpath = C / f'results/{task:03d}.json'
        if not recordpath.exists(): recordpath = C / f'provisional/{task:03d}.json'
        record = json.loads(recordpath.read_text()); bind(recordpath)
        state = classify(record, recordpath.parent.name == 'results')
        assert state['status'] in ('checked_UNSAT', 'pending_validation'), state
        states[arm] = state
        work[arm] = seal['arms']['31/' + arm]['work']
        path = C / f'profiles/{task:03d}-callpaths.txt'
        with path.open() as stream: reports[arm] = read_report(stream, True)
    scopes = {}
    for name, (cs, ns) in SCOPES.items():
        c = select(reports['candidate'], 'candidate', cs)
        n = select(reports['kissat'], 'kissat', ns)
        scopes[name] = dict(candidate=c, kissat=n,
            sampled_excess_cycles=c['estimated_cycles'] - n['estimated_cycles'])
    totals = {arm: r['total_cycles'] for arm, r in reports.items()}
    excess = totals['candidate'] - totals['kissat']
    for p in (Path(__file__), ROOT / 'tools/bench/perf_symbol_cost.py',
              ROOT / 'tools/bench/test_perf_symbol_cost.py'):
        bind(p)
    out = dict(observed_at=dt.datetime.now(dt.timezone.utc).isoformat(), hashes=hashes,
        states=states, work=work, total_sampled_cycles=totals, scopes=scopes,
        inprocessing_fraction_of_sampled_excess=scopes['inprocessing']['sampled_excess_cycles']/excess,
        decision='Current Rank31 excess is not confined to inprocessing. Keep substantial inprocessing cost work, but also investigate excess search work; scope totals do not prove intrinsic CDCL unit-cost inferiority. The shorter Rank43 native/current full decision diagnostic distinguishes earlier divergence from later extra BVE/Factor work.',
        limitations='Rounded sample-period estimates from one concurrent diagnostic window, not replicated acceptance timings, exact event counts, or equal-operation prices. Ordinary propagation overlaps inprocessing; Vivify/Factor/Congruence are nested within it. Never add these envelopes. Rank43 name-only tables contain duplicate clone names and are not accepted by the strict symbol parser; no duplicate rows are dropped or merged. All four solver replays remain valid, independently of this narrower cost interpretation.',
        test_consideration='Existing seven strict perf-symbol parser tests and eight current replay rejection controls cover this reuse. No parser, solver, observer or acceptance predicate changed; no new solver test or duplicate fuzz required.')
    with TARGET.open('x') as stream: json.dump(out, stream, indent=2)
    print(TARGET)
    for name, row in scopes.items():
        print(name, {arm: round(row[arm]['estimated_cycles']/1e9, 3) for arm in ('candidate','kissat')})
    print('inprocessing share of sampled excess', out['inprocessing_fraction_of_sampled_excess'])


if __name__ == '__main__': main()

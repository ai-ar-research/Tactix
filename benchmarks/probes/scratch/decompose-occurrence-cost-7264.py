#!/usr/bin/env python3
"""Conserve all saved phase costs; distinguish local savings from whole cost."""
import json
import math
from pathlib import Path
import statistics
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha

PHASES = {'initial_probe', 'probe', 'eliminate', 'no_phase_frame'}

def partition(candidate, previous):
    assert set(candidate) == set(previous) and candidate
    assert all(type(v) is int and v >= 0 for arm in (candidate, previous) for v in arm.values())
    totals = {arm:sum(values.values()) for arm,values in (('candidate',candidate),('previous',previous))}
    assert totals['candidate'] > 0 and totals['previous'] > 0
    rows = {key:dict(candidate=candidate[key], previous=previous[key],
        delta=candidate[key]-previous[key],
        ratio=candidate[key]/previous[key] if previous[key] else None,
        previous_share=previous[key]/totals['previous'],
        saving_fraction_of_previous_total=(previous[key]-candidate[key])/totals['previous'])
        for key in sorted(previous)}
    delta = totals['candidate'] - totals['previous']
    assert sum(row['delta'] for row in rows.values()) == delta
    return dict(totals=totals, delta=delta, ratio=totals['candidate']/totals['previous'], rows=rows)

def self_symbols(values, arm):
    suffix = f' [{arm}]'
    out = {}
    for name,value in values.items():
        key = name[:-len(suffix)] + ' [solver]' if name.endswith(suffix) else name
        assert key not in out, 'ambiguous solver-DSO label normalization'
        out[key] = value
    assert sum(out.values()) == sum(values.values())
    return out

def union_partition(candidate, previous):
    keys = set(candidate) | set(previous)
    return partition({key:candidate.get(key,0) for key in keys},
                     {key:previous.get(key,0) for key in keys})

def fixtures():
    p = partition({'a':8,'b':13}, {'a':10,'b':10})
    assert p['delta'] == 1 and p['ratio'] == 1.05
    assert p['rows']['a']['saving_fraction_of_previous_total'] == .1
    assert p['rows']['b']['saving_fraction_of_previous_total'] == -.15
    assert union_partition({'a':2}, {'b':3})['rows']['a']['ratio'] is None
    assert self_symbols({'fn [candidate]':2,'fn [libc.so]':3},'candidate') == {'fn [solver]':2,'fn [libc.so]':3}
    for a,b in (({},{}),({'a':1},{'b':1}),({'a':-1},{'a':1}),
                ({'a':True},{'a':1}),({'a':math.nan},{'a':1}),({'a':1},{'a':0})):
        try: partition(a,b)
        except AssertionError: pass
        else: raise AssertionError('invalid partition accepted')
    try: self_symbols({'f [candidate]':1,'f [solver]':2},'candidate')
    except AssertionError: pass
    else: raise AssertionError('ambiguous normalization accepted')
    print('Occurrence decomposition: additive gain/offset, zero baseline, DSO labels and seven invalid controls PASS')

def main():
    assert sys.argv[1:] in ([], ['--self-test'])
    fixtures()
    if sys.argv[1:]: return
    target = root / 'benchmarks/reports/2026-09-11-occurrence-cost-decomposition-7264.json'
    assert not target.exists(), 'successful writer is frozen'
    paths = [root / f'benchmarks/runs/20260911-7260-occurrence-samples/wave{wave}/summary.json' for wave in range(3)]
    if not all(p.exists() for p in paths):
        print('Three extracted profile pairs pending; no output created'); return
    hashes, waves = {}, []
    def bind(p,expected=None):
        digest = sha(p); assert expected is None or digest == expected, p
        hashes[str(p.relative_to(root))] = digest
    for wave,p in enumerate(paths):
        bind(p); r = json.loads(p.read_text()); assert r['wave'] == wave
        for rel,digest in r['hashes'].items(): bind(root/rel,digest)
        arms = r['arms']
        for arm in ('candidate','previous'):
            value = arms[arm]
            assert set(value['phase_periods']) == PHASES
            assert sum(value['phase_periods'].values()) == value['total_period']
            assert not value['unresolved_callers']
            for phase in PHASES - {'no_phase_frame'}:
                assert sum(value['components'][phase].values()) == value['phase_periods'][phase]
                assert sum(value['self_by_phase'][phase].values()) == value['phase_periods'][phase]
        phases = partition(arms['candidate']['phase_periods'],arms['previous']['phase_periods'])
        components, self_deltas, nested = {}, {}, {}
        for phase in sorted(PHASES - {'no_phase_frame'}):
            components[phase] = union_partition(arms['candidate']['components'][phase],arms['previous']['components'][phase])
            own = union_partition(*(self_symbols(arms[arm]['self_by_phase'][phase],arm) for arm in ('candidate','previous')))
            assert own['totals'] == components[phase]['totals']
            self_deltas[phase] = sorted((dict(symbol=key,**value) for key,value in own['rows'].items()),
                key=lambda row:abs(row['delta']),reverse=True)[:15]
            nested[phase] = {}
            for symbol in ('tactix::rebuild_dense_large_occurrences(tactix::Kernel&, tactix::OccIndex&)',
                           'tactix::kernel_rebuild_large_watches(tactix::Kernel&, bool)'):
                costs = {arm:arms[arm]['inclusive_by_phase'][phase].get(symbol,0) for arm in ('candidate','previous')}
                nested[phase][symbol] = dict(**costs,ratio=costs['candidate']/costs['previous'] if costs['previous'] else None)
        waves.append(dict(wave=wave,phases=phases,components=components,
            largest_self_deltas=self_deltas,nested_rebuilds=nested))
    def distribution(values):
        assert len(values) == 3
        return dict(values=values,median=statistics.median(values),min=min(values),max=max(values))
    aggregate = {phase:{metric:distribution([w['phases']['rows'][phase][metric] for w in waves])
        for metric in ('ratio','previous_share','saving_fraction_of_previous_total')}
        for phase in sorted(PHASES)}
    bind(Path(__file__))
    result = dict(hashes=hashes,waves=waves,aggregate=aggregate,
        interpretation='Each phase delta partitions the entire sampled solver cost. Local Eliminate savings as a fraction of the previous total are an explanatory projection, not measured unprofiled speedup. Components partition a phase; nested rebuild costs and inclusive function scopes overlap and must not be added.',
        limitations='Three matched Rank35 sampled profiles, not full50 cost ceilings or proof acceptance. No-phase-frame includes all costs outside identified phase ancestry and is not synonymous with CDCL. Only exact solver DSO suffixes are normalized for self-symbol comparison; inlining differences still limit individual-symbol causality.')
    with target.open('x') as out: json.dump(result,out,indent=2)
    print(target)
    print(json.dumps(aggregate,indent=2))

if __name__ == '__main__': main()

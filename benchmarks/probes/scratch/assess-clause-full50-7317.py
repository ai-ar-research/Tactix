#!/usr/bin/env python3
"""Retain the complete full50 memory tradeoff and rank the remaining gaps."""
import json
import math
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha

def assess(rows):
    assert len(rows) == 50 and {r['rank'] for r in rows} == set(range(1, 51))
    for r in rows:
        assert r['exact_parent_work_and_proof'] is True
        for key in ('candidate_peak_rss_kib', 'parent_peak_rss_kib', 'kissat_peak_rss_kib'):
            assert type(r[key]) is int and r[key] > 0
        for key in ('candidate_seconds', 'parent_seconds', 'kissat_seconds'):
            assert type(r[key]) in (int, float) and math.isfinite(r[key]) and r[key] > 0
    data = [dict(rank=r['rank'], shelved=r['rank'] == 21,
        rss_delta_kib=r['candidate_peak_rss_kib'] - r['parent_peak_rss_kib'],
        native_rss_excess_kib=r['candidate_peak_rss_kib'] - r['kissat_peak_rss_kib'],
        runtime_delta_seconds=r['candidate_seconds'] - r['parent_seconds'],
        native_runtime_excess_seconds=r['candidate_seconds'] - r['kissat_seconds']) for r in rows]
    totals = {key: math.fsum(r[key] for r in rows) for key in ('candidate_seconds','parent_seconds','kissat_seconds')}
    peak = max(rows, key=lambda r: r['candidate_peak_rss_kib'])
    debug = [r for r in data if not r['shelved']]
    return dict(totals=totals, native_ratio=totals['candidate_seconds']/totals['kissat_seconds'],
        aggregate_delta_seconds=totals['candidate_seconds']-totals['parent_seconds'],
        target_remaining_seconds=totals['candidate_seconds']-1.01*totals['kissat_seconds'],
        memory_changes=dict(lower=sum(r['rss_delta_kib'] < 0 for r in data),
            equal=sum(r['rss_delta_kib'] == 0 for r in data), higher=sum(r['rss_delta_kib'] > 0 for r in data)),
        maximum_rss=dict(rank=peak['rank'], kib=peak['candidate_peak_rss_kib'], gib=peak['candidate_peak_rss_kib']/1048576),
        above_16gib=[r['rank'] for r in rows if r['candidate_peak_rss_kib'] > 16*1048576],
        per_rank_deltas=data,
        memory_debug_priority=sorted(debug, key=lambda r:-r['native_rss_excess_kib'])[:10],
        runtime_debug_priority=sorted(debug, key=lambda r:-r['native_runtime_excess_seconds'])[:10],
        largest_memory_regressions=sorted(data, key=lambda r:-r['rss_delta_kib'])[:10])

def fixtures():
    rows = [dict(rank=i, exact_parent_work_and_proof=True, candidate_peak_rss_kib=100+i,
        parent_peak_rss_kib=125, kissat_peak_rss_kib=100, candidate_seconds=i,
        parent_seconds=1, kissat_seconds=1) for i in range(1,51)]
    out = assess(rows)
    assert out['memory_changes'] == dict(lower=24, equal=1, higher=25)
    assert out['totals']['candidate_seconds'] == 1275
    assert len(out['per_rank_deltas']) == 50 and out['per_rank_deltas'][20]['shelved']
    assert not any(r['rank'] == 21 for r in out['memory_debug_priority'])
    invalid = [rows[:-1], rows[:-1]+[rows[0]]]
    for key, value in (('candidate_peak_rss_kib', True), ('candidate_seconds', float('nan')),
                       ('candidate_seconds', 0), ('exact_parent_work_and_proof', False)):
        bad = [dict(r) for r in rows]; bad[0][key] = value; invalid.append(bad)
    for bad in invalid:
        try: assess(bad)
        except AssertionError: pass
        else: raise AssertionError('invalid full50 input accepted')
    print('Full50 assessment: all-rank conservation, shelving only for debugging, six invalid controls PASS')

def main():
    assert sys.argv[1:] in ([], ['--self-test'], ['--seal'])
    fixtures()
    if '--self-test' in sys.argv: return
    target = root / 'benchmarks/reports/2026-09-11-clause-full50-assessment-7317.json'
    if '--seal' in sys.argv: assert not target.exists(), 'successful assessment frozen'
    p = root / 'benchmarks/reports/2026-09-11-clause-realloc-full50-7312/summary.json'
    raw = json.loads(p.read_text())
    hashes = {str(p.relative_to(root)): sha(p)}
    for rel, digest in raw['hashes'].items():
        assert sha(root / rel) == digest, rel
        hashes[rel] = digest
    for p in (Path(__file__), p.with_name('comparison.tsv')):
        hashes[str(p.relative_to(root))] = sha(p)
    result = assess(raw['rows'])
    assert abs(result['totals']['candidate_seconds']-raw['score']['candidate_par2']) < 1e-8
    assert abs(result['totals']['kissat_seconds']-7239.010) < 1e-8
    result.update(hashes=hashes,
        decision='Mixed memory result, no runtime gain: preserve all50 and every regression. Rank25 remains the largest unshelved absolute memory excess;7315 compares exact current/native memory externally. Rank31 remains the largest unshelved runtime gap; phase-aligned work decomposition is required before unit-cost attribution. No production promotion or tiny-peak-chasing change.',
        limitation='One cross-window full-set transfer, not causal speedup attribution.7309 owns repeated paired timing and Rank25 absolute-spread failure. Independent checks/zero residue, current-source fuzz and consolidation remain open. Rank21 remains scored; only debugging priority shelves it.')
    print({key: result[key] for key in ('totals','native_ratio','target_remaining_seconds','memory_changes','maximum_rss','above_16gib')})
    if '--seal' in sys.argv:
        with target.open('x') as out: json.dump(result, out, indent=2)
        print(target)

if __name__ == '__main__': main()

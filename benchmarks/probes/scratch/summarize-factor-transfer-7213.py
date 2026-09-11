#!/usr/bin/env python3
"""Preserve all50 results and contextualize, not discard, the largest deltas."""
import json
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha


def perf(lines):
    out = {}
    for line in lines:
        if not line or line.startswith('#'):
            continue
        fields = line.split('\t')
        assert len(fields) >= 5
        name = fields[2]
        assert name not in out and fields[0] not in ('<not counted>', '<not supported>')
        value = float(fields[0])
        assert value >= 0 and value < float('inf')
        out[name] = value
    assert all(k in out for k in ('task-clock', 'cycles', 'instructions', 'page-faults'))
    return out


def fixtures():
    rows = [f'1\t\t{key}\t1\t100.00' for key in
            ('task-clock', 'cycles', 'instructions', 'page-faults')]
    assert perf(rows)['page-faults'] == 1
    for bad in (rows[:-1], rows + [rows[0]], ['broken'],
                [rows[0].replace('1\t', '-1\t', 1), *rows[1:]],
                [rows[0].replace('1\t', 'nan\t', 1), *rows[1:]],
                [rows[0].replace('1\t', '<not counted>\t', 1), *rows[1:]]):
        try:
            perf(bad)
        except (AssertionError, ValueError):
            pass
        else:
            raise AssertionError('invalid counters accepted')
    print('7213: complete counter context and six negative fixtures PASS')


def main():
    fixtures()
    if '--self-test' in sys.argv:
        return
    target = root / 'benchmarks/reports/2026-09-11-factor-transfer-context-7213.json'
    assert not target.exists(), 'successful summary frozen'
    authority = root / 'benchmarks/reports/2026-09-11-factor-arena-full50-7201/summary.json'
    s = json.loads(authority.read_text())
    hashes = dict(s['hashes'])
    for rel, digest in hashes.items():
        assert sha(root / rel) == digest, rel
    for p in (authority, Path(__file__)):
        hashes[str(p.relative_to(root))] = sha(p)
    rows = s['rows']
    assert len(rows) == 50 and [r['rank'] for r in rows] == list(range(1, 51))
    assert s['exact_parent_work_and_proofs']
    top = sorted(rows, key=lambda r: r['candidate_seconds'] - r['parent_seconds'], reverse=True)[:3]
    assert [r['rank'] for r in top] == [10, 13, 8]
    context = {}
    for row in top:
        context[str(row['rank'])] = {}
        for arm, campaign in (('candidate', '20260911-7201-factor-arena-ranks1-50'),
                              ('parent', '20260910-7167-vivify-revalidation-ranks1-50')):
            p = root / 'benchmarks/runs' / campaign / f"results/{row['rank']-1:03d}.json"
            r = json.loads(p.read_text())
            assert r['valid'] and r['verdict'] == r['expected'] == 'SAT'
            hashes[str(p.relative_to(root))] = sha(p)
            context[str(row['rank'])][arm] = dict(
                record=str(p.relative_to(root)), node=r['node'], solve=r['solve'],
                counters=perf(r['runtime_context']['perf_statistics']),
                factor_statistics=[line for line in r['solver_statistics'] if line.startswith('c factor')],
                timing=[line for line in r['solver_statistics'] if line.startswith('c timing')])
    total = sum(r['candidate_seconds'] - r['parent_seconds'] for r in rows)
    top_delta = sum(r['candidate_seconds'] - r['parent_seconds'] for r in top)
    assert abs(total - 31.800) < 1e-8 and abs(top_delta - 78.331) < 1e-8
    memory = [dict(rank=r['rank'], candidate_kib=r['candidate_peak_rss_kib'],
                   parent_kib=r['parent_peak_rss_kib'],
                   ratio=r['candidate_peak_rss_kib']/r['parent_peak_rss_kib']) for r in rows]
    out = dict(hashes=hashes, score=s['score'], delta_seconds=total,
        largest_three_delta_seconds=top_delta, other47_delta_seconds=total-top_delta,
        selected_context=context, all50_memory=memory,
        decision='Retain isolated arena evidence, not aggregate-gain acceptance. Three repeated matched8/10/13 pairs in7212 distinguish repeatable binary effects from cross-window context. No rank excluded from score; no native refresh.',
        limitation='Page-fault increases correlate with three slowdowns; they do not identify allocation, THP, NUMA, system pressure or source as cause. perf counters include kernel execution. One full-set transfer is not causal attribution. Several RSS increases remain real reported tradeoffs; final checks/fuzz/consolidation/target open.')
    with target.open('x') as stream:
        json.dump(out, stream, indent=2)
        stream.write('\n')
    print(target)
    print('Full50 delta', total, 's; largest three', top_delta, 's; all ranks retained')


if __name__ == '__main__':
    main()

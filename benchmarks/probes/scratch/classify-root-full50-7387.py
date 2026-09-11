#!/usr/bin/env python3
"""Exact observed exceptions to 7385, without changing its strict classifier."""
import copy
import datetime as dt
import importlib.util
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify


def storage_module():
    spec = importlib.util.spec_from_file_location('storage7350', Path(__file__).with_name('root-storage-statistics-7350.py'))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def observed_deactivation(a, b, before, after, storage):
    prefix = 'c deactivate  '
    def line(n):
        return f'{prefix}{n} deactivations, 0 repeated reports, 0 re-bumps of retired vars'
    assert [s for s in a['solver_statistics'] if s.startswith(prefix)] == [line(before)]
    assert [s for s in b['solver_statistics'] if s.startswith(prefix)] == [line(after)]
    adjusted = dict(b, solver_statistics=[line(before) if s == line(after) else s for s in b['solver_statistics']])
    assert storage.compare(a, adjusted)['storage_only'], 'additional work/certificate change'
    return dict(parent_line=line(before), candidate_line=line(after), delta=after-before,
                interpretation='Exact observed counter delta; remaining differences storage-only. Producer not traced on this rank.')


def witness_equal(a, b, input_a, input_b):
    assert input_a == input_b
    for key in ('verdict', 'certificate_sha256', 'certificate_bytes', 'binary_sha256'):
        assert a[key] == b[key], key
    assert [s for s in a['solver_statistics'] if not s.startswith('c timing')] == [s for s in b['solver_statistics'] if not s.startswith('c timing')]


def fixtures(storage):
    a = dict(instance='x', verdict='UNSAT', certificate_sha256='a', certificate_bytes=1,
             binary_sha256='pin', solver_statistics=[
        'c conflicts   3', 'c retired     0 vars (0 substituted, 0 eliminated) of 100',
        'c compact     2 passes removed 10 internal variables',
        'c extension   9 vars added, 4 decisions on them (20.00% of 20)',
        'c denseocc    8 slice moves, 0 defrags',
        'c deactivate  10 deactivations, 0 repeated reports, 0 re-bumps of retired vars'])
    b = dict(a, solver_statistics=[s.replace('10 deactivations', '13 deactivations') for s in a['solver_statistics']])
    assert observed_deactivation(a, b, 10, 13, storage)['delta'] == 3
    rejected = 0
    for old, new in [('13 deactivations', '14 deactivations'), ('0 repeated', '1 repeated'),
                     ('conflicts   3', 'conflicts   4'), ('removed 10', 'removed 11')]:
        bad = dict(b, solver_statistics=[s.replace(old, new) for s in b['solver_statistics']])
        try: observed_deactivation(a, bad, 10, 13, storage)
        except AssertionError: rejected += 1
    for bad in (dict(b, certificate_sha256='bad'), dict(b, solver_statistics=b['solver_statistics']*2)):
        try: observed_deactivation(a, bad, 10, 13, storage)
        except AssertionError: rejected += 1
    assert rejected == 6
    witness_equal(a, dict(a, instance='alias'), 'input', 'input')
    rejected = 0
    for key, value in [('certificate_sha256', 'bad'), ('certificate_bytes', 2),
                       ('binary_sha256', 'other'), ('verdict', 'SAT'), ('solver_statistics', [])]:
        try: witness_equal(a, dict(a, **{key:value}), 'input', 'input')
        except AssertionError: rejected += 1
    try: witness_equal(a, a, 'input', 'other')
    except AssertionError: rejected += 1
    assert rejected == 6
    print('Exact observed deactivation and cross-tier witness: 12 negative controls PASS')


def main():
    assert sys.argv[1:] in ([], ['--self-test'], ['--seal'])
    storage = storage_module(); storage.fixtures(); fixtures(storage)
    if sys.argv[1:] == ['--self-test']: return
    target = ROOT / 'benchmarks/reports/2026-09-11-root-full50-classification-7387.json'
    if '--seal' in sys.argv: assert not target.exists(), 'successful snapshot is frozen'
    hashes = {}
    def bind(p, expected=None):
        digest = sha(p); assert expected is None or digest == expected, str(p)
        hashes[str(p.relative_to(ROOT))] = digest
    def read(p):
        bind(p); return json.loads(p.read_text())
    bind(Path(__file__))
    bind(Path(__file__).with_name('root-storage-statistics-7350.py'))
    summary = read(ROOT / 'benchmarks/reports/2026-09-11-corrected-root-full50-7385/summary.json')
    for rel, digest in summary['hashes'].items(): bind(ROOT / rel, digest)
    c = ROOT / 'benchmarks/runs/20260911-7385-corrected-root-ranks1-50'
    m = read(c / 'manifest.json'); parent = ROOT / m['parent_comparison']
    assert {int(k) for k,v in summary['parent_differences'].items() if v['classification'] == 'CHANGED_WORK_OR_CERTIFICATE'} == {22,34,41}
    exceptions = {}; states = {}
    def record(rank):
        final = c / f'results/{rank-1:03d}.json'
        p = final if final.exists() else c / f'provisional/{rank-1:03d}.json'
        r = read(p); state = classify(r, final.exists())
        assert state['status'] in ('pending_validation', 'checked_SAT', 'checked_UNSAT'), (rank,state)
        states[rank] = state
        return r
    records = {rank:record(rank) for rank in range(1,51)}
    for rank, before, after in [(22,3833330,3833333), (34,15306,15307)]:
        old = read(parent / f'results/{rank-1:03d}.json')
        exceptions[rank] = observed_deactivation(old, records[rank], before, after, storage)
    short = ROOT / 'benchmarks/runs/20260911-7380-corrected-root-pair'
    sm = read(short / 'manifest.json'); folded = read(short / 'results/001.json')
    legacy = read(short / 'results/000.json')
    assert classify(folded, True)['checked'] and classify(legacy, True)['checked']
    inp = m['input_sha256'][records[41]['instance']]
    sinp = sm['input_sha256'][folded['instance']]
    for r in (records[41], folded): bind(ROOT / 'benchmarks/cache' / r['instance'], inp)
    witness_equal(records[41], folded, inp, sinp)
    assert storage.compare(legacy, folded)['storage_only']
    exceptions[41] = dict(interpretation='Exact corrected B14 witness, not a new unexplained trajectory. Real full-view regression remains scored.',
        compressed_input_sha256=inp, certificate_sha256=folded['certificate_sha256'],
        corrected_legacy_seconds=legacy['solve']['wall_seconds'], corrected_folded_seconds=folded['solve']['wall_seconds'])
    rows = copy.deepcopy(summary['rows'])
    for r in rows:
        r['native_gap_seconds'] = r['candidate_seconds']-r['kissat_seconds']
        r['parent_delta_seconds'] = r['candidate_seconds']-r['parent_seconds']
        r['native_rss_excess_kib'] = r['candidate_peak_rss_kib']-r['kissat_peak_rss_kib']
    result = dict(observed_at=dt.datetime.now(dt.timezone.utc).isoformat(), hashes=hashes,
        exceptions=exceptions, checked=sum(s['checked'] for s in states.values()), states=states,
        largest_unshelved_time_gaps=sorted((r for r in rows if r['rank'] != 21), key=lambda r:-r['native_gap_seconds'])[:10],
        largest_unshelved_memory_excess=sorted((r for r in rows if r['rank'] != 21), key=lambda r:-r['native_rss_excess_kib'])[:10],
        parent_delta_seconds=sum(r['parent_delta_seconds'] for r in rows),
        limitation='Sidecar classification only. No altered acceptance gate, discarded rank, isolated unit-cost inference or final pending-proof acceptance.')
    print('Full50 checked:', result['checked'], '; all 3 exceptions classified; all scores retained')
    if '--seal' in sys.argv:
        with target.open('x') as out: json.dump(result, out, indent=2)
        print(target)


if __name__ == '__main__': main()

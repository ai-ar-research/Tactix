#!/usr/bin/env python3
"""Require full own replay and exact completed core before watch interpretation."""
from collections import Counter
import datetime as dt
import importlib.util
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
from native_work import primary_statistics
from runtime_summary import counters
from search_window import read_search, macro_replay
from watch_observer import watch_replay, watch_payload

C = ROOT / 'benchmarks/runs/20260911-7408-rank43-selected-watch'
BASE = ROOT / 'benchmarks/runs/20260911-7397-current-rank43-decisions'
CORE = ROOT / 'benchmarks/runs/20260911-7403-rank43-native-analysis-complete/core-summary-7406.json'
TARGET = C / 'solver-summary-7409.json'


def module(filename):
    p = Path(__file__).with_name(filename)
    spec = importlib.util.spec_from_file_location(filename, p)
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    return m


def selected_snapshot(rows):
    selected = [r for r in rows if (r['conflicts'], r['decisions'], r['level']) == (81, 4048, 115)]
    assert selected, 'missing pre-C82 selected scan'
    # One complete snapshot, no merging separate visits under one conflict.
    assert len({r['propagations'] for r in selected}) == 1, 'multiple selected scans'
    assert all(b['seq'] == a['seq'] + 1 for a, b in zip(selected, selected[1:])), 'noncontiguous snapshot'
    return [dict(seq=r['seq'], **watch_payload(r['payload'], -1038)) for r in selected]


def fixtures():
    def row(seq=1, **kw):
        return dict(seq=seq, conflicts=81, decisions=4048, level=115, propagations=12,
                    payload='scan=-1038|block=-49957|clause=-49957,1038', **kw)
    assert len(selected_snapshot([row(), row(2)])) == 2  # preserve duplicates
    for rows in ([], [dict(row(), conflicts=80)], [row(), row(3)],
                 [row(), dict(row(2), propagations=13)], [dict(row(), payload='scan=-1038')]):
        try:
            selected_snapshot(rows)
        except (AssertionError, ValueError):
            pass
        else:
            raise AssertionError('invalid/merged snapshot accepted')
    print('Pre-C82 snapshot: duplicate positive and five negative controls PASS')


def validate(seal=False):
    if seal:
        assert not TARGET.exists()
    paths = {}
    for task, arm in enumerate(('candidate', 'kissat')):
        p = C / f'results/{task:03d}.json'
        if not p.exists():
            p = C / f'provisional/{task:03d}.json'
        if not p.exists():
            print(arm, 'solver record pending')
            return
        paths[arm] = p
    hashes = {}
    def bind(p, expected=None):
        d = sha(p)
        assert expected is None or d == expected, str(p)
        hashes[str(p.relative_to(ROOT))] = d
    def read(p):
        bind(p)
        return json.loads(p.read_text())
    manifest = read(C / 'manifest.json')
    authority = read(CORE)
    baseline = read(BASE / 'solver-summary-7398.json')
    for rel, d in manifest['pin_sha256'].items():
        bind(C / rel, d)
    for group in ('dependencies', 'protected_campaign_hashes'):
        for rel, d in manifest[group].items():
            bind(ROOT / rel, d)
    for rel, d in authority['hashes'].items():
        bind(ROOT / rel, d)
    bind(ROOT / authority['core_helper'], authority['core_helper_sha256'])
    core = module('rank43-complete-core-7406.py')
    normal = module('validate-square-phase-5486.py')
    normal.self_test()
    for p in (Path(__file__), C / 'launch.json', Path(__file__).with_name('validate-square-phase-5486.py')):
        bind(p)
    results = {}
    snapshots = {}
    for task, arm in enumerate(('candidate', 'kissat')):
        p = paths[arm]
        record = read(p)
        state = classify(record, p.parent.name == 'results')
        assert state['status'] in ('pending_validation', 'checked_UNSAT'), state
        assert (record['task'], record['rank'], record['arm']) == (task, 43, arm)
        assert record['solve']['solver_environment'] == manifest['arms'][arm]['environment']
        old = read(ROOT / baseline['records'][arm]['path'])
        for key in ('instance', 'expected', 'verdict', 'certificate_sha256', 'certificate_bytes'):
            assert record[key] == old[key], (arm, key)
        assert counters(record) == counters(old)
        if arm == 'candidate':
            assert [s for s in record['solver_statistics'] if not s.startswith('c timing')] == [s for s in old['solver_statistics'] if not s.startswith('c timing')]
        else:
            assert primary_statistics(record['solver_statistics']) == primary_statistics(old['solver_statistics'])
        if p.parent.name == 'results':
            assert record['binary_sha256'] == manifest['arms'][arm]['sha256']
        bp = C / 'traces' / f'{arm}.boundaries.tsv'
        dp = C / 'traces' / f'{arm}.decisions.tsv'
        bind(dp, sha(BASE / 'traces' / dp.name))
        bind(bp)
        info = dict(whole_work_certificate_decisions_exact=True)
        if arm == 'candidate':
            assert sha(bp) == sha(BASE / 'traces' / bp.name)
            mp = C / 'traces/candidate.macro.tsv'
            bind(mp)
            info['macro'] = macro_replay((BASE / 'traces/candidate.macro.tsv').read_text(), mp.read_text())
        else:
            def boundaries(path):
                assert path.read_bytes().endswith(b'\n')
                return [l.split('\t') for l in path.read_text().splitlines() if not l.startswith('#')]
            info['analysis_end_additions'] = normal.validate_native_boundaries(boundaries(BASE / 'traces' / bp.name), boundaries(bp), (1, 82))
        campaign = '20260911-7401-rank43-analysis-window' if arm == 'candidate' else '20260911-7403-rank43-native-analysis-complete'
        original = ROOT / 'benchmarks/runs' / campaign / 'traces' / f'{arm}.search.tsv'
        sp = C / 'traces' / original.name
        bind(sp)
        solver = 'tactix' if arm == 'candidate' else 'kissat'
        prior = core.complete_core(original.read_text(), solver=solver, window=(1, 85), cap=250000 if arm == 'candidate' else 500000)
        observed = read_search(sp.read_text(), solver=solver, window=(1, 82), cap=250000)
        detail = [r for r in observed if r['conflicts'] <= 82]
        added = watch_replay(prior, detail, scan=-1038, window=(1, 82))
        snapshots[arm] = selected_snapshot(added)
        results[arm] = dict(record=str(p.relative_to(ROOT)), state=state, replay=info, rows=len(observed),
                            exact_core_rows=len(prior), added_watch_rows=len(added), pre_conflict82_entries=len(snapshots[arm]))
    comparisons = {}
    for clause in ('-49957,1038', '1038,49959'):
        comparisons[clause] = {a: [i for i, r in enumerate(rr) if r['clause'] == clause] for a, rr in snapshots.items()}
    out = dict(observed_at=dt.datetime.now(dt.timezone.utc).isoformat(), hashes=hashes, records=results,
               full_solver_and_core_replay=True, snapshots=snapshots, conflicting_binary_positions=comparisons,
               limitation='Ordered membership/pose before scan, not values for binary blockers. Do not infer simultaneous falsity, unique cause, or a repair from order alone. Original traces and all duplicate entries retained; independent proofs and cleanup remain separate gates. No timing claim.')
    if seal:
        with TARGET.open('x') as stream:
            json.dump(out, stream, indent=2)
    print(json.dumps(dict(records=results, conflicting_binary_positions=comparisons), indent=2))


if __name__ == '__main__':
    assert sys.argv[1:] in ([], ['--self-test'], ['--seal'])
    fixtures()
    if sys.argv[1:] != ['--self-test']:
        validate(sys.argv[1:] == ['--seal'])

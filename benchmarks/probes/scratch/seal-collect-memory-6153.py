#!/usr/bin/env python3
"""Attribute GC intervals only after checked full-run and capacity replay."""
import importlib.util
import json
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[3]
spec = importlib.util.spec_from_file_location('slot_memory',
    Path(__file__).with_name('seal-publication-memory-6131.py'))
base = importlib.util.module_from_spec(spec)
spec.loader.exec_module(base)
STAGES = ('entry', 'before-reserve', 'before-sort', 'after-sort', 'after-copy',
          'after-swap', 'after-reindex', 'exit')
CAPACITY_KEYS = ('vars', 'arena_words', 'arena_capacity', 'watch_words', 'watch_capacity',
    'watch_heads_capacity', 'reconstruction_words', 'reconstruction_capacity',
    'identity_capacity_bytes', 'search_capacity_bytes', 'input_capacity_bytes',
    'touch_capacity_bytes', 'event_capacity_bytes', 'probing_capacity_bytes',
    'proof_index_capacity_bytes', 'heuristics_capacity_bytes', 'clause_alias_capacity_bytes',
    'binary_liveness_capacity_bytes', 'dense_capacity_bytes', 'dense_allocations',
    'policy_reduce_capacity_bytes', 'policy_inprocess_capacity_bytes', 'delayed_capacity_bytes')

def collections(rows):
    groups, pending = [], []
    for row in rows:
        if row['name'] != 'Collect':
            assert not pending, 'interleaved collector'
            continue
        assert row['stage'] == STAGES[len(pending)], row
        pending.append(row)
        if len(pending) != len(STAGES):
            continue
        old_bytes = pending[0]['arena_capacity'] * 4
        new_bytes = pending[5]['arena_capacity'] * 4
        for index, sample in enumerate(pending):
            expected = 0 if index < 2 or index == 7 else new_bytes if index < 5 else old_bytes
            assert sample['collector_temporary_capacity_bytes'] == expected, (index, sample)
            assert sample['arena_capacity'] * 4 == (old_bytes if index < 5 else new_bytes)
        groups.append(pending)
        pending = []
    assert not pending and groups, 'missing/incomplete collectors'
    return groups

def self_test():
    import copy
    valid = [dict(name='Collect', stage=stage, arena_capacity=100 if i < 5 else 20,
                  collector_temporary_capacity_bytes=0 if i < 2 or i == 7 else 80 if i < 5 else 400)
             for i, stage in enumerate(STAGES)]
    assert len(collections(valid + valid)) == 2
    invalid = [[], valid[:-1], valid[1:], valid[:2] + valid[1:],
               valid[:2] + [dict(name='BVE', stage='end')] + valid[2:]]
    bad = copy.deepcopy(valid); bad[-1]['collector_temporary_capacity_bytes'] = 400; invalid.append(bad)
    bad = copy.deepcopy(valid); bad[5]['collector_temporary_capacity_bytes'] = 80; invalid.append(bad)
    bad = copy.deepcopy(valid); bad[4]['arena_capacity'] = 20; invalid.append(bad)
    for rows in invalid:
        try:
            collections(rows)
        except AssertionError:
            pass
        else:
            raise AssertionError('accepted malformed collector sequence')
    base.self_test()
    print('PASS: eight collector sequence/ownership rejection controls')

def main(task=None):
    assert task is None or 0 <= task < 3, 'invalid inspection task'
    c = root / 'benchmarks/runs/20260908-6148-collect-memory'
    paths = [c / 'results' / f'{item:03d}.json' for item in (range(3) if task is None else (task,))]
    missing = [p.name for p in paths if not p.exists()]
    if missing:
        print('Pending final records:', ','.join(missing)); return
    m = json.loads((c / 'manifest.json').read_text())
    for rel, digest in m['pin_sha256'].items():
        assert base.sha(c / rel) == digest, rel
    hashes = {}
    def bind(p):
        hashes[str(p.relative_to(root))] = base.sha(p)
    for name in ('manifest.json', 'launch.json', 'array.sbatch', 'matrix.tsv'):
        bind(c / name)
    records = []
    for p in paths:
        r = json.loads(p.read_text())
        assert base.classify(r, True)['checked'] and r['verdict'] == 'SAT'
        assert r['task'] == int(p.stem) and r['binary_sha256'] == m['arms']['tactix']['sha256']
        release_path = root / f"benchmarks/runs/20260908-6115-substitute-publication-ranks1-50/results/{r['rank']-1:03d}.json"
        assert base.sha(release_path) == m['release_records'][str(release_path.relative_to(root))]
        release = json.loads(release_path.read_text())
        assert base.classify(release, True)['checked'] and r['instance'] == release['instance']
        assert base.logical_statistics(r) == base.logical_statistics(release)
        for field in ('certificate_sha256', 'certificate_bytes'):
            assert r[field] == release[field] and r[field] is not None, field
        provisional = c / 'provisional' / p.name
        pr = json.loads(provisional.read_text())
        for field in ('solve', 'solver_statistics', 'certificate_sha256', 'certificate_bytes'):
            assert pr[field] == r[field], field
        evidence = r['solve']['memory_trace']
        trace = c / 'logs' / f"{r['task']:03d}.slot-memory.tsv"
        assert evidence['path'] == str(trace.relative_to(c))
        assert base.sha(trace) == evidence['sha256'] and trace.stat().st_size == evidence['bytes']
        rows = base.parse_trace(trace.read_text())
        groups = collections(rows)
        assert all(a['max_rss_kib'] <= b['max_rss_kib'] for a, b in zip(rows, rows[1:]))
        old = root / f"benchmarks/runs/20260908-6129-publication-memory/logs/{r['task']:03d}.slot-memory.tsv"
        old_summary = root / 'benchmarks/runs/20260908-6129-publication-memory/summary.json'
        assert base.sha(old) == json.loads(old_summary.read_text())['hashes'][str(old.relative_to(root))]
        old_rows = base.parse_trace(old.read_text())
        slots = [row for row in rows if row['name'] != 'Collect']
        def project(row):
            return (row['name'], row['stage'], *(row[key] for key in CAPACITY_KEYS))
        assert list(map(project, old_rows)) == list(map(project, slots)), 'changed slot capacity/work trajectory'
        peak = max(rows, key=lambda row: row['max_rss_kib'])
        peak_index = rows.index(peak)
        containing = [group for group in groups if group[0]['row'] <= peak['row'] <= group[-1]['row']]
        records.append(dict(rank=r['rank'], rows=len(rows), collections=len(groups),
            checked_clean=True, exact_full_logical_statistics=True, exact_emitted_proof=True,
            exact_old_slot_capacity_trajectory=True, first_peak_sample=peak,
            previous_sample=rows[peak_index-1] if peak_index else None,
            peak_collection=containing[0] if containing else None,
            observed_peak_rss_kib=r['solve']['peak_rss_kb']))
        for path in (p, provisional, trace, release_path, old, old_summary):
            bind(path)
    if task is not None:
        print(json.dumps(dict(read_only_single_task=True, records=records), indent=2))
        return
    out = c / 'summary.json'
    assert not out.exists()
    out.write_text(json.dumps(dict(records=records, hashes=hashes,
        limitation='Observed memory attribution,not Release timing or hard-memory acceptance. Collector temporary capacity is the other clause arena only,not all scratch. A peak first seen at entry precedes collector allocation. Intervals do notidentify unsampled suboperations.'), indent=2) + '\n')
    print('PASS: three checked/clean full-work/proof and old-slot-capacity replays; collector intervals sealed')

if __name__ == '__main__':
    if '--self-test' in sys.argv:
        self_test()
    elif len(sys.argv) == 3 and sys.argv[1] == '--inspect-task':
        main(int(sys.argv[2]))
    else:
        assert len(sys.argv) == 1
        main()

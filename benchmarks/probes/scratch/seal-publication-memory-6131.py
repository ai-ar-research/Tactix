#!/usr/bin/env python3
"""Seal only checked, cleaned, full-work-replayed slot-memory diagnostics."""
import csv
import hashlib
import json
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from checked_comparison import classify

sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
KEYS = ('current_rss_kib', 'max_rss_kib', 'dense_capacity_bytes',
        'heap_used_kib', 'heap_free_kib', 'mmap_kib',
        'arena_capacity', 'watch_capacity', 'reconstruction_capacity')


def parse_trace(text):
    rows = []
    fields_at_entry = None
    for number, line in enumerate(text.splitlines(), 1):
        fields = line.split('\t')
        assert len(fields) >= 3 and fields[0] and fields[1], number
        values = {}
        for field in fields[2:]:
            key, value = field.split('=')
            assert key not in values and key not in ('row', 'name', 'stage'), key
            assert value.isdecimal(), (key, value)
            values[key] = int(value)
        assert set(KEYS) <= values.keys(), number
        if fields_at_entry is None:
            fields_at_entry = set(values)
        assert set(values) == fields_at_entry, number
        rows.append(dict(row=number, name=fields[0], stage=fields[1], **values))
    assert rows, 'empty trace'
    return rows


def logical_statistics(record):
    rows = [s for s in record['solver_statistics'] if not s.startswith('c timing')]
    assert any(s.startswith('c conflicts ') for s in rows)
    assert any(s.startswith('c decisions ') for s in rows)
    assert any(s.startswith('c propagations ') for s in rows)
    return rows


def self_test():
    line = 'Eliminate\tend\t' + '\t'.join(f'{k}=7' for k in KEYS)
    assert len(parse_trace(line + '\n' + line)) == 2
    for text in ('', line + '\tdense_capacity_bytes=1', line.replace('=7', '=-1', 1),
                 line.replace('=7', '=1.5', 1), 'Eliminate\tend\tx=1',
                 line + '\n' + line + '\textra=1'):
        try:
            parse_trace(text)
        except (AssertionError, ValueError):
            pass
        else:
            raise AssertionError('accepted malformed trace')
    a = dict(solver_statistics=['c conflicts 1', 'c decisions 2', 'c propagations 3', 'c timing 4'])
    b = dict(solver_statistics=a['solver_statistics'][:-1] + ['c timing 5'])
    assert logical_statistics(a) == logical_statistics(b)
    b['solver_statistics'][1] = 'c decisions 3'
    assert logical_statistics(a) != logical_statistics(b)
    print('PASS: six negative trace fixtures and exact-work/timing controls')


def main():
    c = root / 'benchmarks/runs/20260908-6129-publication-memory'
    m = json.loads((c / 'manifest.json').read_text())
    for rel, digest in m['pin_sha256'].items():
        assert sha(c / rel) == digest, rel
    with (c / 'matrix.tsv').open(newline='') as stream:
        matrix = list(csv.DictReader(stream, delimiter='\t'))
    assert [int(r['rank']) for r in matrix] == [21, 25, 9]
    missing = [r['task'] for r in matrix if not (c / 'results' / f"{int(r['task']):03d}.json").exists()]
    if missing:
        print('Pending final records:', ','.join(missing))
        return
    hashes = {}
    def bind(path):
        hashes[str(path.relative_to(root))] = sha(path)
    for name in ('manifest.json', 'launch.json', 'matrix.tsv', 'array.sbatch'):
        bind(c / name)
    release = {}
    for rel, digest in m['release_records'].items():
        path = root / rel
        assert sha(path) == digest, rel
        r = json.loads(path.read_text())
        assert classify(r, True)['checked']
        release[r['rank']] = r
        bind(path)
    summaries = []
    for row in matrix:
        path = c / 'results' / f"{int(row['task']):03d}.json"
        r = json.loads(path.read_text())
        assert classify(r, True)['checked'], r
        assert r['binary_sha256'] == m['arms']['tactix']['sha256']
        for field in ('arm', 'instance', 'expected'):
            assert r[field] == row[field], field
        assert r['task'] == int(row['task']) and r['rank'] == int(row['rank'])
        baseline = release[r['rank']]
        assert r['instance'] == baseline['instance'] and r['verdict'] == baseline['verdict'] == 'SAT'
        assert logical_statistics(r) == logical_statistics(baseline), ('observer changed work', r['rank'])
        for field in ('certificate_sha256', 'certificate_bytes'):
            assert r[field] == baseline[field] and r[field] is not None, field
        provisional = c / 'provisional' / path.name
        pr = json.loads(provisional.read_text())
        for field in ('solve', 'certificate_sha256', 'certificate_bytes', 'solver_statistics'):
            assert pr[field] == r[field], field
        evidence = r['solve']['memory_trace']
        trace = c / 'logs' / f"{r['task']:03d}.slot-memory.tsv"
        assert evidence['path'] == str(trace.relative_to(c))
        assert sha(trace) == evidence['sha256'] and trace.stat().st_size == evidence['bytes']
        rows = parse_trace(trace.read_text())
        for p in (path, provisional, trace):
            bind(p)
        summaries.append(dict(rank=r['rank'], instance=r['instance'], rows=len(rows),
            exact_non_timing_statistics=True, exact_emitted_proof=True, model_independently_checked=True,
            release_peak_rss_kib=baseline['solve']['peak_rss_kb'],
            observed_peak_rss_kib=r['solve']['peak_rss_kb'],
            peaks={key: max(rows, key=lambda x: x[key]) for key in KEYS}))
    result = dict(records=summaries, hashes=hashes, checked_clean=3,
                  limitation=m['limitations'])
    target = c / 'summary.json'
    if target.exists():
        assert json.loads(target.read_text()) == result
    else:
        target.write_text(json.dumps(result, indent=2) + '\n')
    print('PASS: allthree checked/clean; exact full logical statistics and emittedproof bytes; memory ownership sealed')


if __name__ == '__main__':
    self_test() if '--self-test' in sys.argv else main()

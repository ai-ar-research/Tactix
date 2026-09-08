#!/usr/bin/env python3
"""Compare every new decision against two contiguous frozen native streams."""
import csv
import hashlib
import json
from pathlib import Path
import shutil
import sys
import tempfile

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from decision_prefix import first_stream_difference, read_prefix
c = root / 'benchmarks/runs/20260908-5837-forward-admission-rank32-prefix'
target = c / 'summary.json'
assert not target.exists()
def sha(p):
    h = hashlib.sha256()
    with p.open('rb') as stream:
        for block in iter(lambda: stream.read(8 << 20), b''):
            h.update(block)
    return h.hexdigest()

def overlap(early, suffix, stop):
    prefix = b''
    count = 0
    first_after = None
    with suffix.open('rb') as stream:
        for line in stream:
            row = list(map(int, line.split()))
            assert len(row) == 5
            if row[0] > stop:
                first_after = row
                break
            assert row[0] == stop
            prefix += line
            count += 1
    with early.open('rb') as stream:
        assert len(prefix) <= early.stat().st_size
        stream.seek(early.stat().st_size - len(prefix))
        assert stream.read() == prefix, 'overlapping stop-conflict rows differ'
    return len(prefix), count, first_after

with tempfile.TemporaryDirectory(prefix='overlap-test-', dir=c) as scratch:
    x, y = Path(scratch) / 'early', Path(scratch) / 'suffix'
    a, b, d = b'9\t1\t1\t1\t0\n', b'10\t2\t2\t2\t0\n', b'11\t3\t3\t3\t0\n'
    x.write_bytes(a + b)
    y.write_bytes(b + d)
    assert overlap(x, y, 10) == (len(b), 1, [11, 3, 3, 3, 0])
    y.write_bytes(d)
    assert overlap(x, y, 10) == (0, 0, [11, 3, 3, 3, 0])
    y.write_bytes(b.replace(b'\t2\t0', b'\t9\t0') + d)
    try:
        overlap(x, y, 10)
    except AssertionError:
        pass
    else:
        raise AssertionError('mismatching cached overlap accepted')
m = json.loads((c / 'manifest.json').read_text())
for rel, digest in m['hashes'].items():
    assert sha(c / rel) == digest
records = {}
for arm, binding in [('early', m['cached_early_native']), ('native', m['cached_controls']['kissat'])]:
    p = root / binding['record']
    assert sha(p) == binding['sha256']
    record = json.loads(p.read_text())
    assert record['exit_code'] == 0 and record['scratch_deleted']
    for rel, digest in record['trace_sha256'].items():
        assert sha(p.parent.parent / 'traces' / rel) == digest
    records[arm] = (p.parent.parent, record)
p = c / 'results/candidate.json'
r = json.loads(p.read_text())
assert r['exit_code'] == 0 and r['scratch_deleted'] and r['binary_sha256'] == sha(c / 'pins/candidate')
for rel, digest in r['trace_sha256'].items():
    assert sha(c / 'traces' / rel) == digest
early_path = records['early'][0] / 'traces/kissat.decisions.tsv'
native_path = records['native'][0] / 'traces/kissat.decisions.tsv'
new_path = c / 'traces/candidate.decisions.tsv'
first = first_stream_difference(new_path, early_path)
prefix = read_prefix(c / 'traces/candidate.prefix.tsv')
native_prefix = read_prefix(records['native'][0] / 'traces/kissat.prefix.tsv')
early_prefix = read_prefix(records['early'][0] / 'traces/kissat.prefix.tsv')
assert prefix['conflict'] == native_prefix['conflict'] == 300000
assert early_prefix['conflict'] == 100000
early_equal = first['right'] is None and first['matching_rows'] == early_prefix['count']
second = None
overlap_rows = 0
if early_equal:
    assert first['matching_rows'] == m['common_decisions'] == prefix['common']
    assert prefix['common_reached'] and prefix['common_hash'] == native_prefix['common_hash']
    overlap_size, overlap_rows, first_native = overlap(early_path, native_path, early_prefix['conflict'])
    assert first['left'] == first_native, 'cached suffix not contiguous with verified early stream'
    # This comparison-only suffix is reproducible from the retained full trace.
    # Own its exact temporary directory and remove it after comparison; do not
    # retain another multi-gigabyte duplicate or alter either input trace.
    with tempfile.TemporaryDirectory(prefix='suffix-compare-', dir=c) as scratch:
        suffix = Path(scratch) / 'candidate.tsv'
        with new_path.open('rb') as src, suffix.open('wb') as dst:
            src.seek(early_path.stat().st_size - overlap_size)
            shutil.copyfileobj(src, dst, 8 << 20)
        second = first_stream_difference(suffix, native_path)
    assert second['matching_rows'] >= overlap_rows
    total_common = first['matching_rows'] + second['matching_rows'] - overlap_rows
else:
    total_common = first['matching_rows']

def boundaries(path):
    with path.open() as stream:
        assert next(stream).startswith(('# tactix-', '# kissat-'))
        header = next(stream).rstrip().removeprefix('# ').split('\t')
        return list(csv.DictReader(stream, fieldnames=header, delimiter='\t'))
selected = {}
for arm, base in [('candidate', c), ('native', records['native'][0])]:
    solver = 'candidate' if arm == 'candidate' else 'kissat'
    selected[arm] = [row for row in boundaries(base / ('traces/' + solver + '.boundaries.tsv'))
                     if row['conflicts'] == '179463' and row['name'].lower() in ('strengthen', 'bve')]
result = dict(result_sha256=sha(p), complete_trace_sha256=r['trace_sha256'],
    new_prefix=prefix, native_prefix=native_prefix, early_comparison=first, suffix_comparison=second,
    matching_decisions=total_common, cached_overlap_rows=overlap_rows, overlap_controls=3, target_boundaries=selected,
    cached_bindings=dict(early=m['cached_early_native'], native=m['cached_controls']['kissat']),
    cleanup='Temporary comparison-only suffix removed;original full trace,cached native streams and solver result retained. Solver-owned scratch cleaned.',
    limitation='Intended scheduling change;first difference is diagnostic,not a runtime benefit or checked final proof. Native streams overlap at conflict100K;the exact matching overlap is verified and counted only once. All full-file hashes and common prefix are checked.')
target.write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps({k: result[k] for k in ('early_comparison', 'suffix_comparison', 'matching_decisions', 'target_boundaries')}, indent=2))

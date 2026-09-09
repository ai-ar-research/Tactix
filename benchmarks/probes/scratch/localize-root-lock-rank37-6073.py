#!/usr/bin/env python3
"""Hash-bound coarse localization; counts never stand in for formula equality."""
from collections import Counter
import csv
import hashlib
import io
import itertools
import json
from pathlib import Path
import sys

NAMES = {'congruence': 'Congruence', 'substitute_complete': 'Substitute',
         'substitute_early': 'Substitute', 'substitute_late': 'Substitute',
         'backbone': 'Backbone', 'backbone_early': 'Backbone', 'backbone_late': 'Backbone',
         'vivify': 'Vivify', 'sweep': 'Sweep', 'transitive': 'TransitiveReduce',
         'factor': 'Factor', 'strengthen': 'Strengthen', 'bve': 'BVE'}
FIELDS = ('name', 'stage', 'conflicts', 'decisions', 'active', 'irred', 'red', 'binary')

def parse(stream, native):
    assert next(stream).startswith('# kissat-inprocess-boundary' if native else '# tactix-inprocess-boundary')
    header = next(stream).rstrip('\n').removeprefix('# ').split('\t')
    assert len(header) == len(set(header)) and all(k in header for k in FIELDS)
    rows, ignored = [], Counter()
    for row in csv.DictReader(stream, fieldnames=header, delimiter='\t'):
        assert None not in row and all(v is not None for v in row.values())
        name = row['name']
        if name not in (NAMES if native else NAMES.values()):
            ignored[name] += 1
            continue
        assert row['stage'] in ('begin', 'end')
        out = {key: (row[key] if key in ('name', 'stage') else int(row[key])) for key in FIELDS}
        assert all(out[key] >= 0 for key in FIELDS if key not in ('name', 'stage'))
        out['name'] = NAMES[name] if native else name
        rows.append(out)
    return rows, dict(ignored)

def compare(left, right, omit=()):
    project = lambda r: None if r is None else {k: v for k, v in r.items() if k not in omit}
    first = None
    for index, (a, b) in enumerate(itertools.zip_longest(left, right)):
        if project(a) != project(b):
            first = dict(index=index, candidate=a, kissat=b,
                         preceding_candidate=left[max(0, index-3):index], preceding_kissat=right[max(0, index-3):index])
            break
    return dict(equal=first is None, candidate_rows=len(left), kissat_rows=len(right), first=first)

def fixtures():
    header = '# solver\tseq\tname\tstage\tconflicts\tdecisions\tpropagations\tactive\tunassigned\ttrail\tirred\tred\tbinary\n'
    row = 'tactix\t0\tSweep\tend\t0\t0\t3\t4\t4\t1\t5\t6\t7\n'
    left, ignored = parse(io.StringIO('# tactix-inprocess-boundary\t1\n' + header + row), False)
    right, _ = parse(io.StringIO('# kissat-inprocess-boundary\t1\n' + header + row.replace('tactix', 'kissat').replace('Sweep', 'sweep')), True)
    assert not ignored and compare(left, right)['equal'] and left[0]['conflicts'] == 0
    changed = [dict(right[0], binary=8)]
    assert compare(left, changed)['first']['index'] == 0
    assert compare(left, changed, ('binary',))['equal']
    assert not compare(left, [])['equal'] and not compare([], right)['equal']
    changed = [dict(right[0], stage='begin')]
    assert not compare(left, changed, ('binary',))['equal']
    skipped, ignored = parse(io.StringIO('# tactix-inprocess-boundary\t1\n' + header + row.replace('Sweep', 'SparseCollect')), False)
    assert not skipped and ignored == {'SparseCollect': 1}
    for bad in (row.rstrip('\n') + '\textra\n', row.replace('\t7\n', '\n'), row.replace('\tend\t', '\tbogus\t'), row.replace('\t7\n', '\t-1\n')):
        try:
            parse(io.StringIO('# tactix-inprocess-boundary\t1\n' + header + bad), False)
        except (AssertionError, ValueError):
            pass
        else:
            raise AssertionError('malformed boundary accepted')
    print('PASS: boundary mapping,startup,binary-only,missing-row,stage,ignored-row and four malformed-row controls')

def main():
    fixtures()
    if '--self-test' in sys.argv:
        return
    root = Path(__file__).resolve().parents[3]
    c = root / 'benchmarks/runs/20260908-6070-root-lock-rank37-prefix'
    target = c / 'boundary-localization.json'
    assert not target.exists()
    sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
    m = json.loads((c / 'manifest.json').read_text())
    summary = c / 'summary.json'
    s = json.loads(summary.read_text())
    assert s['new_prefix']['conflict'] == 1000000
    native = root / m['cached_controls']['kissat']['record']
    assert sha(native) == m['cached_controls']['kissat']['sha256']
    rows, skipped, hashes = {}, {}, {str(summary.relative_to(root)): sha(summary)}
    for arm, record in (('candidate', c / 'results/candidate.json'), ('kissat', native)):
        r = json.loads(record.read_text())
        assert r['exit_code'] == 0 and r['scratch_deleted']
        path = record.parent.parent / 'traces' / (arm + '.boundaries.tsv')
        assert sha(path) == r['trace_sha256'][path.name]
        hashes[str(path.relative_to(root))] = sha(path)
        with path.open() as stream:
            rows[arm], skipped[arm] = parse(stream, arm == 'kissat')
    search = {arm: [r for r in rs if r['conflicts'] > 0] for arm, rs in rows.items()}
    out = dict(all=compare(rows['candidate'], rows['kissat']), search=compare(search['candidate'], search['kissat']),
               nonbinary_search=compare(search['candidate'], search['kissat'], ('binary',)), ignored_names=skipped, hashes=hashes,
               decision_difference=s['suffix_comparison'] if s['suffix_comparison'] else s['early_comparison'],
               limitation='Coarse ordinal-aligned shared mechanism boundaries only. Startup included separately; binary-only differences remain unresolved,not dismissed. Equal counts do not prove formula,pose,effort or relative-strength parity. Propagation,trail,ticks and native-only phase wrappers are not aligned fields.')
    target.write_text(json.dumps(out, indent=2) + '\n')
    print(json.dumps(out, indent=2))

if __name__ == '__main__':
    main()

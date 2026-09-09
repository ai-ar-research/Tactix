#!/usr/bin/env python3
"""Read published results; seal exact stage-one work/proof neutrality once."""
import csv
import json
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify

def compare_pair(a, b):
    for key in ('rank', 'instance', 'expected', 'verdict', 'certificate_sha256', 'certificate_bytes'):
        assert a[key] == b[key], key
    for r in (a, b):
        assert r['expected_match'] and r['verdict'] == r['expected']
        assert r['solve']['exit_code'] == {'SAT': 10, 'UNSAT': 20}[r['expected']]
        assert not r['solve']['timed_out']
        assert isinstance(r['solver_statistics'], list) and r['solver_statistics']
        assert all(isinstance(line, str) for line in r['solver_statistics'])
        if r['expected'] == 'UNSAT':
            assert r['certificate_bytes'] > 0 and len(r['certificate_sha256']) == 64
    non_timing = lambda r: [line for line in r['solver_statistics'] if not line.startswith('c timing ')]
    assert non_timing(a) == non_timing(b), 'non-timing solver statistics differ'
    return dict(non_timing_lines=len(non_timing(a)), certificate_sha256=a['certificate_sha256'],
                certificate_bytes=a['certificate_bytes'])

def fixtures():
    from copy import deepcopy
    a = dict(rank=1, instance='fixture', expected='UNSAT', verdict='UNSAT', expected_match=True,
             certificate_sha256='a' * 64, certificate_bytes=12,
             solve=dict(exit_code=20, timed_out=False),
             solver_statistics=['c conflicts 7', 'c timing 9 ms solve', 's UNSATISFIABLE'])
    b = deepcopy(a)
    b['solver_statistics'][1] = 'c timing 18 ms solve'
    assert compare_pair(a, b)['non_timing_lines'] == 2
    negatives = []
    for key, value in (('rank', 2), ('instance', 'other'), ('expected', 'SAT'),
                       ('verdict', 'SAT'), ('certificate_sha256', 'b' * 64),
                       ('certificate_bytes', 13), ('expected_match', False),
                       ('solver_statistics', [])):
        x = deepcopy(a)
        x[key] = value
        negatives.append(x)
    for change in (dict(exit_code=0), dict(timed_out=True)):
        x = deepcopy(a)
        x['solve'].update(change)
        negatives.append(x)
    x = deepcopy(a)
    x['solver_statistics'][0] = 'c conflicts 8'
    negatives.append(x)
    for x in negatives:
        try:
            compare_pair(a, x)
        except AssertionError:
            pass
        else:
            raise AssertionError('invalid neutral control accepted')
    print('PASS one timing-only positive and eleven negative control fixtures')

def main():
    fixtures()
    if '--self-test' in sys.argv:
        return
    c = root / 'benchmarks/runs/20260909-6574-material-state-BC-gate'
    target = c / 'neutrality-6576.json'
    if '--seal' in sys.argv:
        assert not target.exists()
    hashes = {}
    def read(path):
        hashes[str(path.relative_to(root))] = sha(path)
        return json.loads(path.read_text())
    m = read(c / 'manifest.json')
    launch = read(c / 'launch.json')
    assert launch['manifest_sha256'] == sha(c / 'manifest.json')
    for rel, digest in m['pin_sha256'].items():
        assert sha(c / rel) == digest, rel
        hashes[str((c / rel).relative_to(root))] = digest
    with (c / 'matrix.tsv').open() as stream:
        matrix = list(csv.DictReader(stream, delimiter='\t'))
    assert len(matrix) == 66 and [int(row['task']) for row in matrix] == list(range(66))
    records, final = {}, {}
    for row in matrix:
        task = int(row['task'])
        p = c / 'provisional' / (str(task).zfill(3) + '.json')
        f = c / 'results' / p.name
        if f.exists():
            r = read(f)
            assert r['binary_sha256'] == m['arms'][row['arm']]['sha256']
            final[task] = classify(r, True)
            assert final[task]['checked'] and final[task]['clean'], (task, final[task])
        if p.exists():
            r = read(p)
            assert r['validation_pending'] and 'valid' not in r
            assert r['rank'] == int(row['rank']) and r['instance'] == row['instance']
            assert r['task'] == task
            # The provisional schema deliberately omits binary_sha256. Its
            # provenance is the immutable matrix/runner/pins above; require
            # the explicit recorded binary identity on every final result.
            records[task] = r
    pairs = {}
    for task in range(0, 66, 2):
        if task in records and task + 1 in records:
            pairs[str(matrix[task]['rank'])] = compare_pair(records[task], records[task + 1])
    print(f'Published {len(records)}/66, neutral pairs {len(pairs)}/33, checked/clean {len(final)}/66')
    if '--seal' not in sys.argv:
        return
    assert len(records) == 66 and len(pairs) == 33, 'solver phase not complete; no summary written'
    for rel in ('benchmarks/probes/scratch/validate-material-state-BC-6576.py',
                'benchmarks/runs/20260909-6571-material-state-build/manifest.json',
                'benchmarks/runs/20260909-6571-material-state-build/parent.json',
                'benchmarks/runs/20260909-6573-material-shared-touch-control/summary.json'):
        hashes[rel] = sha(root / rel)
    with target.open('x') as stream:
        json.dump(dict(hashes=hashes, pairs=pairs, all_work_and_certificates_equal=True,
            checked_clean=len(final), validation_pending=len(final) != 66,
            tooling_failure='First read-only status invocation expected binary_sha256 in the intentionally smaller provisional schema and raised KeyError before writing any summary. Corrected to bind immutable task inputs and verify explicit binary identity on every final record; no solver failure or exclusion.',
            limitation='All reported non-timing statistics and emitted certificate bytes/hashes agree across33 full paired solves. This is not complete internal-state identity, byte-identical SAT models, runtime acceptance or promotion. Independent pending proof checks remain obligations. Scheduling consumers are not enabled.'), stream, indent=2)
        stream.write('\n')
    print(target)

if __name__ == '__main__':
    main()

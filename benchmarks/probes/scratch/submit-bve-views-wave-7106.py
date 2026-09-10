#!/usr/bin/env python3
"""Submit one independent six-task wave; require exact own and cross-arm work."""
import csv
import datetime as dt
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify

def fingerprint(r):
    lines = r['solver_statistics']
    assert isinstance(lines, list) and lines and all(isinstance(s, str) for s in lines)
    return tuple(r[key] for key in ('certificate_sha256', 'certificate_bytes', 'verdict', 'instance')) + (
        tuple(s for s in lines if not s.startswith('c timing')),)

def fixtures():
    r = dict(certificate_sha256='a', certificate_bytes=1, verdict='UNSAT', instance='x',
             solver_statistics=['c conflicts 2', 'c timing 3'])
    assert fingerprint(r) == fingerprint(dict(r, solver_statistics=['c conflicts 2', 'c timing 8']))
    for key, value in [('certificate_sha256', 'b'), ('certificate_bytes', 2),
                       ('verdict', 'SAT'), ('instance', 'y'),
                       ('solver_statistics', ['c conflicts 3'])]:
        assert fingerprint(r) != fingerprint(dict(r, **{key: value}))
    for value in ([], 'c conflicts 2', [1]):
        try: fingerprint(dict(r, solver_statistics=value))
        except AssertionError: pass
        else: raise AssertionError('invalid statistics accepted')
    print('own-repeat fingerprint: timing exclusion, five changes, three invalid schemas PASS')

def main():
    fixtures()
    if sys.argv[1:] == ['--self-test']: return
    wave = int(sys.argv[1]); assert wave in (0, 1, 2) and len(sys.argv) == 2
    c = root / 'benchmarks/runs/20260910-7105-bve-views-runtime'
    target = c / f'wave{wave}-submission.json'
    assert not target.exists(), 'already submitted; monitor existing job'
    m = json.loads((c / 'manifest.json').read_text())
    for rel, digest in m['pin_sha256'].items(): assert sha(c / rel) == digest, rel
    for group in ('protected_campaign_hashes', 'dependencies'):
        for rel, digest in m[group].items(): assert sha(root / rel) == digest, rel
    for binding in m['authorities'].values(): assert sha(root / binding['path']) == binding['sha256']
    gate = root / 'benchmarks/runs/20260910-7103-bve-views-BC-gate'
    for folder in ('provisional', 'results'):
        for p in (gate / folder).glob('*.json'):
            assert classify(json.loads(p.read_text()), folder == 'results')['status'] in ('pending_validation', 'checked_SAT', 'checked_UNSAT'), p
    with (c / 'matrix.tsv').open() as stream: matrix = list(csv.DictReader(stream, delimiter='\t'))
    assert len(matrix) == 18 and [int(r['task']) for r in matrix] == list(range(18))
    rows, paths = {}, {}
    for folder in ('provisional', 'results'):
        for p in sorted((c / folder).glob('*.json')):
            r = json.loads(p.read_text()); task = r['task']
            assert task == int(p.stem) and task in range(18)
            assert classify(r, folder == 'results')['status'] in ('pending_validation', 'checked_UNSAT'), p
            for key in ('arm', 'instance', 'expected'): assert r[key] == matrix[task][key], (p, key)
            assert r['rank'] == int(matrix[task]['rank'])
            if folder == 'results': assert r['binary_sha256'] == m['arms'][r['arm']]['sha256']
            if task in rows:
                assert fingerprint(r) == fingerprint(rows[task]) and r['solve'] == rows[task]['solve']
            rows[task], paths[task] = r, p
    assert set(rows) == set(range(6 * wave)), 'preceding solver wave incomplete'
    first = {(r['arm'], r['rank']): r for task, r in rows.items() if task < 6}
    for task, r in rows.items():
        assert fingerprint(r) == fingerprint(first[(r['arm'], r['rank'])]), (task, 'own work/proof changed')
        if r['arm'] in ('previous', 'candidate'):
            binding = m['previous_work_oracles'][str(r['rank'])]
            assert sha(root / binding['path']) == binding['sha256']
            assert fingerprint(r) == fingerprint(json.loads((root / binding['path']).read_text())), (task, 'arm differs from7030')
    since = dt.datetime.now().replace(microsecond=0).isoformat()
    command = ['sbatch', '--parsable', f'--array={6*wave}-{6*wave+5}', str(c / 'array.sbatch')]
    job = subprocess.check_output(command, text=True).strip().split(';')[0]
    assert job.isdigit()
    with target.open('x') as stream:
        json.dump(dict(job_id=job, tasks=list(range(6*wave, 6*wave+6)), command=command,
            accounting_since=since, manifest_sha256=sha(c / 'manifest.json'),
            helper_sha256=sha(Path(__file__)), preceding_records={str(p.relative_to(root)): sha(p) for p in paths.values()}), stream, indent=2)
        stream.write('\n')
    print(job, since)

if __name__ == '__main__': main()

#!/usr/bin/env python3
"""Read-only exact runtime validation, or once-only submission of one wave."""
import csv
import datetime as dt
import importlib.util
import json
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify

def identity(r, row):
    assert r['task'] == int(row['task'])
    assert r['rank'] == int(row['rank'])
    for key in ('arm', 'instance', 'expected'): assert r[key] == row[key], key

def fixtures():
    row = dict(task='0', rank='31', arm='candidate', instance='x', expected='UNSAT')
    r = dict(row, task=0, rank=31); identity(r, row)
    for key, value in (('task', 1), ('rank', 35), ('arm', 'previous'), ('instance', 'y'), ('expected', 'SAT')):
        try: identity(dict(r, **{key: value}), row)
        except AssertionError: pass
        else: raise AssertionError('bad identity accepted: ' + key)
    print('Factor runtime identity: positive and five mismatches PASS')

def main():
    assert sys.argv[1:] == ['--self-test'] or not sys.argv[1:] or (len(sys.argv) == 3 and sys.argv[1] == '--submit')
    fixtures()
    if sys.argv[1:] == ['--self-test']: return
    c = root / 'benchmarks/runs/20260910-7186-factor-arena-runtime'
    spec = importlib.util.spec_from_file_location('fingerprints', c / 'pins/exact_fingerprint.py')
    h = importlib.util.module_from_spec(spec); spec.loader.exec_module(h); h.fixtures()
    m = json.loads((c / 'manifest.json').read_text())
    for rel, digest in m['pin_sha256'].items(): assert sha(c / rel) == digest, rel
    for rel, digest in m['dependencies'].items(): assert sha(root / rel) == digest, rel
    subprocess.run([sys.executable, str(Path(__file__).with_name('check-submit-factor-arena-BC-7183.py'))], check=True)
    with (c / 'matrix.tsv').open() as stream: matrix = list(csv.DictReader(stream, delimiter='\t'))
    assert len(matrix) == 18 and [int(r['task']) for r in matrix] == list(range(18))
    records, paths, states = {}, {}, {}
    for folder in ('provisional', 'results'):
        for p in sorted((c / folder).glob('*.json')):
            r = json.loads(p.read_text()); task = r['task']
            assert task == int(p.stem) and task in range(18)
            identity(r, matrix[task])
            state = classify(r, folder == 'results')
            assert state['status'] in ('pending_validation', 'checked_UNSAT'), (p, state)
            if folder == 'results': assert r['binary_sha256'] == m['arms'][r['arm']]['sha256']
            if task in records:
                assert h.fingerprint(r) == h.fingerprint(records[task]) and r['solve'] == records[task]['solve']
            records[task], paths[task], states[task] = r, p, state
    for task, r in records.items():
        oracle = m['previous_work_oracles'][str(r['rank'])]
        assert sha(root / oracle['path']) == oracle['sha256']
        assert h.fingerprint(r) == h.fingerprint(json.loads((root / oracle['path']).read_text())), task
        first = next((v for i,v in records.items() if i < 6 and v['rank'] == r['rank'] and v['arm'] == r['arm']), None)
        assert first and h.fingerprint(r) == h.fingerprint(first), task
    for task in range(0, 18, 2):
        if task in records and task+1 in records: assert h.fingerprint(records[task]) == h.fingerprint(records[task+1])
    print('Factor runtime:', len(records), '/18 exact outcomes;', sum(s['checked'] for s in states.values()), 'checked-clean')
    for task, r in sorted(records.items()):
        print(task, r['rank'], r['arm'], r['solve']['wall_seconds'], 's', r['solve']['peak_rss_kb'], 'KiB')
    if not sys.argv[1:]: return
    wave = int(sys.argv[2]); assert wave in (0, 1, 2)
    assert set(records) == set(range(6*wave)), 'preceding wave incomplete or next wave already started'
    target = c / f'wave{wave}-submission.json'; assert not target.exists(), 'already submitted; monitor existing job'
    since = dt.datetime.now().replace(microsecond=0).isoformat()
    command = ['sbatch', '--parsable', f'--array={6*wave}-{6*wave+5}', str(c / 'array.sbatch')]
    job = subprocess.check_output(command, text=True).strip().split(';')[0]; assert job.isdigit()
    with target.open('x') as out:
        json.dump(dict(job_id=job, accounting_since=since, tasks=list(range(6*wave, 6*wave+6)), command=command,
            manifest_sha256=sha(c / 'manifest.json'), helper_sha256=sha(Path(__file__)),
            preceding_records={str(p.relative_to(root)): sha(p) for p in paths.values()}), out, indent=2)
    print(job, since)

if __name__ == '__main__': main()

#!/usr/bin/env python3
"""Freeze the user-selected existing 12-CPU reference; never launch a solver."""
import csv
import datetime as dt
import hashlib
import json
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
SOURCE = ROOT / 'benchmarks/runs/20260908-5712-assignment-level-ranks1-40'
TARGET = ROOT / 'benchmarks/baselines/rank1-40-kissat-12cpu-v1'


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def authority(path):
    return dict(path=str(path.relative_to(ROOT)), sha256=sha(path))


def validate(row, record):
    for key in ('arm', 'instance', 'expected'):
        if record[key] != row[key]:
            raise ValueError('record identity mismatch: ' + key)
    if int(record['rank']) != int(row['rank']) or int(record['task']) != int(row['task']):
        raise ValueError('record rank/task mismatch')
    s = record['solve']
    if not record['expected_match'] or s['timed_out'] or s['exit_code'] != {'SAT': 10, 'UNSAT': 20}[row['expected']]:
        raise ValueError('reference is not an expected completed solve')
    if not math.isfinite(s['wall_seconds']) or s['wall_seconds'] <= 0:
        raise ValueError('invalid reference time')
    if 'valid' in record and not record['valid']:
        raise ValueError('failed reference certificate')


def main():
    if TARGET.exists():
        raise SystemExit('Frozen reference already exists; refusing replacement')
    m = json.loads((SOURCE / 'manifest.json').read_text())
    resources = m['resource_contract']
    assert resources['cpus_per_task'] == 12 and resources['memory_mib'] == 48000
    assert resources['solve_timeout'] == 1200 and resources['one_solver_per_task']
    assert resources['proof_mode'] == 'DRAT on both arms'
    for name, digest in m['pin_sha256'].items():
        assert sha(SOURCE / name) == digest, name
    with (SOURCE / 'matrix.tsv').open(newline='') as stream:
        rows = [r for r in csv.DictReader(stream, delimiter='\t') if r['arm'] == 'kissat']
    assert len(rows) == 40 and {int(r['rank']) for r in rows} == set(range(1, 41))
    records = []
    for row in rows:
        task = int(row['task'])
        path = SOURCE / 'provisional' / f'{task:03}.json'
        r = json.loads(path.read_text())
        validate(row, r)
        final_path = SOURCE / 'results' / f'{task:03}.json'
        final = json.loads(final_path.read_text()) if final_path.exists() else None
        if final is not None:
            validate(row, final)
            assert final['binary_sha256'] == m['arms']['kissat']['sha256']
            for key in ('solve', 'certificate_sha256', 'certificate_bytes'):
                assert final[key] == r[key], key
        records.append(dict(rank=int(row['rank']), instance=row['instance'],
            input_sha256=m['input_sha256'][row['instance']], expected=row['expected'],
            measurement=r, measurement_authority=authority(path),
            checked_at_freeze=bool(final and final['valid']),
            final_authority=authority(final_path) if final else None))
    total = math.fsum(r['measurement']['solve']['wall_seconds'] for r in records)
    assert math.isclose(total, 6810.066, rel_tol=0, abs_tol=1e-8)
    manifest = dict(schema='tactix-fixed-rank1-40-kissat-reference-v1',
        frozen_at=dt.datetime.now(dt.timezone.utc).isoformat(),
        selection='Latest complete pre-request Rank1-40 Kissat vector, not best-of; user requested reuse at fixed 12 CPUs.',
        source_campaign=str(SOURCE.relative_to(ROOT)), source_manifest=authority(SOURCE / 'manifest.json'),
        source_checkpoint=authority(SOURCE / 'solver-checkpoint.json'),
        solver=m['arms']['kissat'], checkers={k:v for k,v in m['pin_sha256'].items()
            if k in ('pins/drat-trim', 'pins/checkmodel', 'pins/modelcheck.py')},
        resource_contract=resources, solver_seconds=total, instances=40,
        checked_at_freeze=sum(r['checked_at_freeze'] for r in records), records=records,
        future_contract='Tactix-only full-width 40-task arrays, same rank/input hashes, proof mode and resource limits. No routine Kissat rerun; existing native diagnostics reusable when observer contracts match.',
        verification='Measurements immutable; outstanding checks continue in source campaign. Consult its final results independently; pending is not verified and failed checks invalidate acceptance.',
        limitation='Cross-window fixed-reference score, not isolated unit-cost attribution; B28+C5, current-source fuzz and memory gates unchanged.')
    TARGET.mkdir(parents=True, exist_ok=False)
    (TARGET / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(f'Frozen {total:.3f}s; {manifest["checked_at_freeze"]}/40 checked; no solver launched')


if __name__ == '__main__':
    main()

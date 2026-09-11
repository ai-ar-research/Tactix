#!/usr/bin/env python3
"""Exact own-work comparison for different Tactix and native solver schemas."""
import importlib.util
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from native_work import primary_statistics
spec = importlib.util.spec_from_file_location('occurrence_contract',
    Path(__file__).with_name('occurrence-fingerprint-7256.py'))
h = importlib.util.module_from_spec(spec)
spec.loader.exec_module(h)

def fingerprint(record, solver):
    if solver == 'tactix':
        return h.raw_fingerprint(record)
    assert solver == 'kissat'
    identity = tuple(record[k] for k in ('certificate_sha256', 'certificate_bytes', 'verdict', 'instance'))
    return identity + (primary_statistics(record['solver_statistics']),)

def fixtures():
    h.fixtures()
    r = dict(certificate_sha256='a', certificate_bytes=1, verdict='SAT', instance='x',
        solver_statistics=['c ---- [ statistics ] ----', 'c conflicts: 7 1.0 per second',
                           'c decisions: 23 ', 'c ---- [ resources ] ----'])
    assert fingerprint(r, 'kissat') == fingerprint(dict(r, solver_statistics=[
        s.replace('1.0', '3.0') for s in r['solver_statistics']]), 'kissat')
    for key, value in (('certificate_sha256', 'b'), ('certificate_bytes', 2),
                       ('verdict', 'UNSAT'), ('instance', 'y')):
        assert fingerprint(r, 'kissat') != fingerprint(dict(r, **{key: value}), 'kissat')
    for old, new in (('conflicts: 7', 'conflicts: 8'), ('decisions: 23', 'decisions: 24')):
        assert fingerprint(r, 'kissat') != fingerprint(dict(r, solver_statistics=[
            s.replace(old, new) for s in r['solver_statistics']]), 'kissat')
    for lines in ([], r['solver_statistics'][:-1], r['solver_statistics'] * 2):
        try:
            fingerprint(dict(r, solver_statistics=lines), 'kissat')
        except ValueError:
            pass
        else:
            raise AssertionError('incomplete native statistics accepted')
    try:
        fingerprint(r, 'tactix')
    except AssertionError:
        pass
    else:
        raise AssertionError('native statistics accepted as Tactix')
    print('Rank25 contract: exact own schemas, six changed-work/certificate controls, three invalid native sections PASS')

if __name__ == '__main__':
    fixtures()

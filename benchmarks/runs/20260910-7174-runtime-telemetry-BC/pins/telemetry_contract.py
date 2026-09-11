#!/usr/bin/env python3
"""Strict fingerprint for the predeclared binary-telemetry/clock ablation."""
import re

ROW = re.compile(r'^c binprop +([0-9]+) scans, ([0-9]+) flagged, ([0-9]+) words, ([0-9]+) skipped, ([0-9]+) residual-ordered, ([0-9]+) residual-large$')

def fingerprint(record, ablated=False):
    lines = record['solver_statistics']
    assert isinstance(lines, list) and lines and all(isinstance(s, str) for s in lines)
    telemetry = [s for s in lines if s.startswith('c binprop')]
    assert len(telemetry) == 1, telemetry
    match = ROW.fullmatch(telemetry[0])
    assert match, telemetry
    if ablated: assert all(int(n) == 0 for n in match.groups()), telemetry
    return tuple(record[k] for k in ('certificate_sha256', 'certificate_bytes', 'verdict', 'instance')) + (
        tuple(s for s in lines if not s.startswith('c timing') and s != telemetry[0]),)

def fixtures():
    zero = 'c binprop     0 scans, 0 flagged, 0 words, 0 skipped, 0 residual-ordered, 0 residual-large'
    nonzero = zero.replace('0 scans', '9 scans')
    base = dict(certificate_sha256='a', certificate_bytes=123, verdict='UNSAT', instance='x',
                solver_statistics=['c conflicts 9', zero, 'c timing 2'])
    other = dict(base, solver_statistics=['c conflicts 9', nonzero, 'c timing 7'])
    assert fingerprint(base, True) == fingerprint(other)
    failures = [dict(base, solver_statistics=ls) for ls in (
        [], zero, [0], ['c conflicts 9'], [zero, zero], [zero + ' unexpected'],
        ['c conflicts 9', nonzero])]
    for bad in failures:
        try: fingerprint(bad, True)
        except AssertionError: pass
        else: raise AssertionError('invalid telemetry accepted')
    for key, value in [('certificate_sha256', 'b'), ('certificate_bytes', 124),
                       ('verdict', 'SAT'), ('instance', 'y'),
                       ('solver_statistics', ['c conflicts 10', zero])]:
        assert fingerprint(dict(base, **{key: value}), True) != fingerprint(base, True)
    print('telemetry contract: timing/counter-only positive, seven malformed/nonzero and five semantic negatives PASS')

if __name__ == '__main__': fixtures()

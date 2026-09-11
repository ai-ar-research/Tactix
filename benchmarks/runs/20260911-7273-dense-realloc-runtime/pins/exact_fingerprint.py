#!/usr/bin/env python3
"""Predeclared cutover comparison: exclude only timing and physical dense moves.

Dense slice moves/defrags are storage telemetry, never policy inputs. Removing
the duplicate owner changes them legitimately. Keep raw rows in every record;
require exact same-arm repetitions and all other work/certificate fields.
"""
import re
PATTERN = re.compile(r'c denseocc    ([0-9]+) slice moves, ([0-9]+) defrags')

def raw_fingerprint(r):
    lines = r['solver_statistics']
    assert isinstance(lines, list) and lines and all(isinstance(s, str) for s in lines)
    dense = [s for s in lines if s.startswith('c denseocc')]
    assert len(dense) == 1 and PATTERN.fullmatch(dense[0]), dense
    return tuple(r[key] for key in ('certificate_sha256', 'certificate_bytes', 'verdict', 'instance')) + (
        tuple(s for s in lines if not s.startswith('c timing')),)

def fingerprint(r):
    strict = raw_fingerprint(r)
    return strict[:-1] + (tuple(s for s in strict[-1] if not s.startswith('c denseocc')),)

def fixtures():
    a = 'c denseocc    17 slice moves, 2 defrags'
    b = 'c denseocc    8 slice moves, 1 defrags'
    r = dict(certificate_sha256='a', certificate_bytes=1, verdict='UNSAT', instance='x',
             solver_statistics=['c conflicts 2', a, 'c timing 3'])
    changed = dict(r, solver_statistics=['c conflicts 2', b, 'c timing 8'])
    assert fingerprint(r) == fingerprint(changed)
    assert raw_fingerprint(r) != raw_fingerprint(changed)
    assert raw_fingerprint(r) == raw_fingerprint(dict(r, solver_statistics=['c conflicts 2', a, 'c timing 9']))
    for key, value in [('certificate_sha256', 'b'), ('certificate_bytes', 2),
                       ('verdict', 'SAT'), ('instance', 'y'),
                       ('solver_statistics', ['c conflicts 3', a]),
                       ('solver_statistics', ['c conflicts 2', a, 'c new-semantic-counter 1'])]:
        assert fingerprint(r) != fingerprint(dict(r, **{key:value}))
    for lines in ([], 'c conflicts 2', [1], ['c conflicts 2'],
                  [a, a], [a, b], ['c denseocc    -1 slice moves, 0 defrags'],
                  ['c denseocc    0 slice moves, bad defrags'],
                  [a + ' trailing'], ['c denseocc    1 slice moves, 2 defrags, 3 work']):
        try: fingerprint(dict(r, solver_statistics=lines))
        except AssertionError: pass
        else: raise AssertionError(('invalid statistics accepted', lines))
    print('occurrence fingerprint: narrow physical counters; exact own repeats; six semantic changes; ten invalid schemas PASS')

if __name__ == '__main__': fixtures()

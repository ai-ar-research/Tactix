#!/usr/bin/env python3
"""Include earlier decisions in the split conflict; preserve all mapped rows."""
import hashlib
import importlib.util
import json
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[3]
spec = importlib.util.spec_from_file_location('boundary', Path(__file__).with_name('localize-root-lock-rank37-6073.py'))
boundary = importlib.util.module_from_spec(spec)
spec.loader.exec_module(boundary)

def before(rows, conflict, decision):
    return [r for r in rows if (r['conflicts'], r['decisions']) < (conflict, decision)]

def align(left, right):
    assert len(left) == len(right)
    fields = ('name', 'stage', 'conflicts', 'decisions')
    assert all(all(a[k] == b[k] for k in fields) for a, b in zip(left, right))
    return [dict(candidate=a, kissat=b, differences={k: [a[k], b[k]] for k in a if a[k] != b[k]})
            for a, b in zip(left, right)]

def fixtures():
    rows = [dict(conflicts=9, decisions=20), dict(conflicts=10, decisions=20),
            dict(conflicts=10, decisions=21), dict(conflicts=10, decisions=22),
            dict(conflicts=11, decisions=22)]
    assert before(rows, 10, 21) == rows[:2]
    assert before(rows, 10, 20) == rows[:1]
    a = dict(name='Sweep', stage='end', conflicts=10, decisions=20, binary=1)
    b = dict(a, binary=2)
    assert align([a, a], [b, a])[0]['differences'] == {'binary': [1, 2]}
    for bad in ([], [dict(b, stage='begin')], [dict(b, decisions=21)]):
        try:
            align([a], bad)
        except AssertionError:
            pass
        else:
            raise AssertionError('mismatched boundary accepted')
    print('PASS: same-conflict before/equal/after cutoff, repeated rounds, count-only change, three alignment rejects')

def main():
    fixtures()
    if '--self-test' in sys.argv:
        return
    c = root / 'benchmarks/runs/20260908-6081-rank37-position-history'
    target = c / 'same-conflict-localization.json'
    assert not target.exists()
    sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
    summary = c / 'summary.json'
    s = json.loads(summary.read_text())
    assert s['parent_full_replay'] and s['common_boundary_count'] == 11804
    rows, hashes = {}, {str(summary.relative_to(root)): sha(summary)}
    for arm in ('candidate', 'kissat'):
        p = c / 'traces' / f'{arm}.boundaries.tsv'
        assert sha(p) == s['evidence'][arm]['traces'][p.name]
        hashes[str(p.relative_to(root))] = sha(p)
        with p.open() as f:
            parsed, _ = boundary.parse(f, arm == 'kissat')
        rows[arm] = [r for r in before(parsed, 272979, 76953349) if r['conflicts'] == 272979]
        assert rows[arm] and all(r['decisions'] == 76952961 for r in rows[arm])
        assert sum(r['name'] == 'Congruence' and r['stage'] == 'begin' for r in rows[arm]) == 2
    pairs = align(rows['candidate'], rows['kissat'])
    result = dict(hashes=hashes, pairs=pairs, rounds=2, split=[272979, 76953349],
        phase_decision=76952961, differences=[p for p in pairs if p['differences']],
        correction='Prior frozen runtime report incorrectly named228591 as last inprocessing. Conflict-only strict cutoff omitted two rounds at272979/76952961 before split76953349.',
        limitation='Mapped portable count boundaries only; identical counts do not prove physical formula/heap identity or equal effort. Full heap brackets6084/6085 also include conflict/backtrack/first decision.')
    target.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({k:v for k,v in result.items() if k not in ('hashes', 'pairs')}, indent=2))

if __name__ == '__main__':
    main()

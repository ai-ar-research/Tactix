#!/usr/bin/env python3
"""Validate observer neutrality and distinguish score order from heap tie pose."""
import json
import math
from pathlib import Path
import runpy
import sys

def heap(content, arm):
    assert content.endswith('\n')
    lines = content.splitlines()
    assert lines[:2] == ['# ' + arm + '-heap-set\t1', '# external\tposition\tscore\tassigned\tactive']
    rows = []
    for i, line in enumerate(lines[2:]):
        f = line.split('\t')
        assert len(f) == 5
        v, pos, score, assigned, active = int(f[0]), int(f[1]), float(f[2]), int(f[3]), int(f[4])
        assert v > 0 and pos == i and math.isfinite(score) and score >= 0
        assert assigned in (0, 1) and active in (0, 1)
        if i: assert rows[(i-1)//2]['score'] >= score
        rows.append(dict(variable=v, position=pos, score=score, assigned=assigned, active=active))
    assert rows and len({r['variable'] for r in rows}) == len(rows)
    return rows

def fixtures():
    good = '# tactix-heap-set\t1\n# external\tposition\tscore\tassigned\tactive\n1\t0\t2\t1\t1\n2\t1\t2\t0\t1\n3\t2\t0\t0\t0\n'
    assert len(heap(good, 'tactix')) == 3
    assert len(heap(good.replace('tactix', 'kissat'), 'kissat')) == 3
    bad = [good.rstrip('\n'), good.replace('set\t1', 'set\t2'),
        good.replace('1\t0\t2', '0\t0\t2'), good.replace('2\t1\t2', '1\t1\t2'),
        good.replace('2\t1\t2', '2\t2\t2'), good.replace('2\t1\t2', '2\t1\t3'),
        good.replace('3\t2\t0', '3\t2\tnan'), good.replace('3\t2\t0', '3\t2\t-1'),
        good.replace('2\t1\t2\t0', '2\t1\t2\t2'), good + '\n',
        good.replace('3\t2\t0\t0\t0', '3\t2\t0\t0\t2'),
        '\n'.join(good.splitlines()[:2])+'\n']
    for content in bad:
        try: heap(content, 'tactix')
        except (AssertionError, ValueError): pass
        else: raise AssertionError('Malformed heap accepted')
    print('PASS: two positive/twelve negative heap fixtures', flush=True)

def main():
    fixtures()
    if '--self-test' in sys.argv: return
    support = runpy.run_path(str(Path(__file__).with_name('compare-small-factor-5566.py')))
    c = support['root'] / 'benchmarks/runs/20260908-5689-rank33-common-heap'
    if any(not (c / 'results' / (a + '.json')).exists() for a in ('candidate','kissat')):
        print('5689 pending'); return
    evidence = support['validate'](c)
    heaps = {a:heap((c / 'traces' / (a + '.heap.tsv')).read_text(), 'tactix' if a == 'candidate' else a)
        for a in ('candidate','kissat')}
    maps = {a:{r['variable']:r for r in rows} for a,rows in heaps.items()}
    t,k = maps['candidate'],maps['kissat']
    common = sorted(t.keys() & k.keys())
    def differences(field): return [v for v in common if t[v][field] != k[v][field]]
    # Compare the two competing choices directly without silently normalizing
    # their IEEE scores. Equalities matter for physical heap tie behavior.
    choices = (120438,121103)
    relation = lambda x,y:(x>y)-(x<y)
    selected = {a:{str(v):mapping.get(v) for v in choices} for a,mapping in maps.items()}
    relative = {a:relation(mapping[choices[0]]['score'],mapping[choices[1]]['score'])
        for a,mapping in maps.items() if all(v in mapping for v in choices)}
    ratios = [t[v]['score']/k[v]['score'] for v in common if k[v]['score']>0]
    result = dict(parent_full_replay=True, parser_fixtures=14, evidence=evidence,
        snapshot=dict(conflict=182093, decisions=49396660),
        heap_sizes={a:len(rows) for a,rows in heaps.items()},
        candidate_only=sorted(t.keys()-k.keys()), kissat_only=sorted(k.keys()-t.keys()),
        differences={f:dict(count=len(differences(f)),first_variables=differences(f)[:20])
            for f in ('score','position','assigned')}, selected=selected,
        selected_score_relation=relative,
        nonzero_reference_score_ratio_range=[min(ratios),max(ratios)] if ratios else None,
        limitation='Active flags are representation-local. A numeric scale difference or heap pose difference alone is not a proven performance defect; compare selected score relations, then localize its producer.')
    p = c / 'summary.json'
    assert not p.exists()
    p.write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({k:v for k,v in result.items() if k != 'evidence'},indent=2))

if __name__ == '__main__': main()

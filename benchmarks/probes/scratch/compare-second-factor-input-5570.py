#!/usr/bin/env python3
"""Compare full second-pass large clauses, physical order, pose and flags."""
from collections import Counter
import json
from pathlib import Path
import runpy
import sys

support = runpy.run_path(str(Path(__file__).with_name('compare-small-factor-5566.py')))
root, sha, validate = (support[k] for k in ('root', 'sha', 'validate'))

def rows(content):
    lines = content.splitlines()
    assert len(lines) >= 2 and lines[0] in ('# tactix-formula-set\t2', '# kissat-formula-set\t2')
    assert lines[1] == '# redundant\tsize\thash\tliterals'
    result = []
    for line in lines[2:]:
        f = line.split('\t')
        assert len(f) == 11
        base = support['formula']('\n'.join(lines[:2] + ['\t'.join(f[:4])]))[0]
        attrs = dict(x.split('=', 1) for x in f[4:])
        assert set(attrs) == {'pose', 'glue', 'priority', 'searched', 'used', 'reason', 'shrunken'}
        pose = tuple(map(int, attrs.pop('pose').split(',')))
        assert tuple(sorted(pose)) == base[1]
        meta = {k:int(v) for k,v in attrs.items()}
        assert all(v >= 0 for v in meta.values())
        assert all(meta[k] in (0, 1) for k in ('priority', 'reason', 'shrunken'))
        assert meta['searched'] < len(pose)
        result.append((base, pose, tuple(sorted(meta.items()))))
    return result

def fixtures():
    good = '# tactix-formula-set\t2\n# redundant\tsize\thash\tliterals\n0\t3\t0000000000000001\t-3,1,2\tpose=1,-3,2\tglue=0\tpriority=0\tsearched=2\tused=0\treason=0\tshrunken=0\n'
    assert rows(good)[0][1] == (1, -3, 2)
    assert len(rows(good + good.splitlines()[-1] + '\n')) == 2
    for bad in ('', good.replace('pose=1,-3,2', 'pose=1,-3,4'),
                good.replace('priority=0', 'priority=2'),
                good.replace('searched=2', 'searched=3'),
                good.replace('used=0', 'used=-1'), good.replace('shrunken=0', 'reason=0')):
        try: rows(bad)
        except (AssertionError, ValueError): pass
        else: raise AssertionError('Malformed pose/meta accepted')
    # Same-conflict snapshots are ordinal-separated, never merged.
    b = '# begin\t0\ttactix\tslot_boundary\t19375\t1\tname=Factor|stage=begin\n0\t-3\t4\n# end\t0\n'
    b += b.replace('\t0\t', '\t1\t').replace('\n0\t', '\n1\t').replace('# end\t0', '# end\t1')
    assert support['binary_snapshots'](b, 'tactix') == [[(-3, 4)], [(-3, 4)]]
    print('PASS: three positive/six negative second-pass parser fixtures', flush=True)

def main():
    fixtures()
    if '--self-test' in sys.argv: return
    c = root / 'benchmarks/runs/20260908-5569-rank32-second-factor-input'
    if any(not (c/'results'/(a+'.json')).exists() for a in ('candidate','kissat')):
        print('5569 pending'); return
    evidence = validate(c)
    first = root / 'benchmarks/runs/20260908-5565-rank32-factor-input'
    data = {}
    for arm in ('candidate','kissat'):
        name = arm+'.binary.tsv'
        assert sha(c/'traces'/name) == sha(first/'traces'/name)
        data[arm] = rows((c/'traces'/(arm+'.formula.tsv')).read_text())
        assert Counter(v[0][0] for v in data[arm]) == {0:553534, 1:2013}
    a,b = data['candidate'],data['kissat']
    # Align physical slots only after reporting equality of the underlying clauses.
    clauses_a,clauses_b = [v[0] for v in a],[v[0] for v in b]
    first_diff = next((dict(index=i,candidate=x,kissat=y)
                      for i,(x,y) in enumerate(zip(a,b)) if x!=y),None)
    result = dict(parent_replay=True, parser_fixtures=9, evidence=evidence,
        binary_replay_5565=True, clause_multisets_equal=Counter(clauses_a)==Counter(clauses_b),
        physical_clause_order_equal=clauses_a==clauses_b,
        complete_pose_meta_equal=a==b, first_physical_difference=first_diff,
        pose_differences=sum(x[1]!=y[1] for x,y in zip(a,b)),
        metadata_differences=sum(x[2]!=y[2] for x,y in zip(a,b)),
        limitation='Pose/metadata slot counts are comparable only when physical clause order agrees. Binary multisets do not include per-literal watch order or factor marks/heap state. No established fix or performance gain.')
    p = c/'input-localization.json'
    if p.exists(): assert json.loads(p.read_text()) == json.loads(json.dumps(result))
    else: p.write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({k:v for k,v in result.items() if k!='evidence'},indent=2))

if __name__ == '__main__': main()

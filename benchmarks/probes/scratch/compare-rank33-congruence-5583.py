#!/usr/bin/env python3
"""Identify actual clause multiplicities behind the early Congruence count gap."""
from collections import Counter
import json
from pathlib import Path
import runpy
import sys
support=runpy.run_path(str(Path(__file__).with_name('compare-small-factor-5566.py')))
root=support['root']

def binary_snapshots(content,arm):
    # Normalize only the boundary label/conflict, retaining all framing/count checks.
    for line in content.splitlines():
        if line.startswith('# begin\t'):
            fields=line.split('\t')
            assert len(fields)==7 and fields[4]=='9457'
            assert (fields[3],fields[6])==(
                ('slot_boundary','name=Congruence|stage=end') if arm=='tactix'
                else ('congruence_end','-'))
    normalized=content.replace('\t9457\t','\t19375\t')
    normalized=normalized.replace('name=Congruence|stage=end','name=Factor|stage=begin')
    normalized=normalized.replace('congruence_end','factor_begin')
    return support['binary_snapshots'](normalized,arm)

def fixtures():
    good='# begin\t0\ttactix\tslot_boundary\t9457\t1\tname=Congruence|stage=end\n0\t-3\t4\n# end\t0\n'
    assert binary_snapshots(good,'tactix')==[[(-3,4)]]
    native=good.replace('tactix','kissat').replace('slot_boundary','congruence_end').replace('name=Congruence|stage=end','-')
    assert binary_snapshots(native,'kissat')==[[(-3,4)]]
    second=good.replace('# begin\t0','# begin\t1').replace('\n0\t','\n1\t').replace('# end\t0','# end\t1')
    assert binary_snapshots(good+second,'tactix')==[[(-3,4)],[(-3,4)]]
    for bad in ('',good.replace('9457','19375'),good.replace('stage=end','stage=begin'),
                good.replace('Congruence','Factor'),good.replace('# end\t0\n','')):
        try:binary_snapshots(bad,'tactix')
        except (AssertionError,ValueError):pass
        else:raise AssertionError('Wrong Congruence snapshot accepted')
    print('PASS: three positive/five negative Congruence boundary fixtures',flush=True)

def main():
    fixtures()
    if '--self-test' in sys.argv:return
    c=root/'benchmarks/runs/20260908-5582-rank33-congruence-output'
    if any(not(c/'results'/(a+'.json')).exists() for a in ('candidate','kissat')):
        print('5582 pending');return
    evidence=support['validate'](c)
    clauses,binaries={},{}
    for arm in ('candidate','kissat'):
        clauses[arm]=support['formula']((c/'traces'/(arm+'.formula.tsv')).read_text())
        assert Counter(v[0] for v in clauses[arm])=={0:583991 if arm=='candidate' else 583986,1:2885}
        binaries[arm]=binary_snapshots((c/'traces'/(arm+'.binary.tsv')).read_text(),'tactix' if arm=='candidate' else arm)
        assert [len(v) for v in binaries[arm]]==[754302,754396]
    ac,bc=(Counter(clauses[a]) for a in ('candidate','kissat'))
    extras={side:[dict(redundant=k[0],literals=k[1],difference=v,
        candidate_copies=ac[k],kissat_copies=bc[k]) for k,v in sorted(diff.items())]
        for side,diff in [('candidate_only',ac-bc),('kissat_only',bc-ac)]}
    result=dict(parent_replay=True,parser_fixtures=8,evidence=evidence,
        large_clause_difference=extras,
        binary_difference_by_pass=[support['multiset_difference'](a,b) for a,b in zip(binaries['candidate'],binaries['kissat'])],
        limitation='Two Congruence outputs share9457; the large snapshot selects the first and binary snapshots retain both. Full output multisets identify remaining clauses and duplicates, not the incoming extraction pose, mark lifecycle or cause of the later182093 decision split. No runtime or stronger-rule claim.')
    p=c/'clause-localization.json'
    if p.exists():assert json.loads(p.read_text())==json.loads(json.dumps(result))
    else:p.write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({k:v for k,v in result.items() if k!='evidence'},indent=2))

if __name__=='__main__':main()

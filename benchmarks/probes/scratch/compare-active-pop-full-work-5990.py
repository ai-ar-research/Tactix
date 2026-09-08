#!/usr/bin/env python3
"""Bound all40 parent comparisons, isolating only the intended two counters."""
import hashlib
import json
from pathlib import Path
import re
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
import fixed_runtime

def split(lines):
    ordinary=[];counters={}
    for line in lines:
        if line.startswith('c timing'):continue
        match=re.match(r'^c (deactivate|bve)\s',line)
        if match:
            assert match[1] not in counters
            counters[match[1]]=line
        else:ordinary.append(line)
    return ordinary,counters

def fixtures():
    a=['c bve 4 eliminated','c bve-bwd 0 units','c decisions 2','c timing 3']
    b=['c bve 3 eliminated','c bve-bwd 0 units','c decisions 2','c timing 4']
    assert split(a)[0]==split(b)[0] and split(a)[1]!=split(b)[1]
    assert split(a+['extra'])[0]!=split(b)[0]
    assert split([x.replace('bve-bwd 0','bve-bwd 1') for x in a])[0]!=split(b)[0]
    print('PASS: exact all-line comparison excludes only two declared counters',flush=True)

def main():
    fixtures()
    if '--self-test' in sys.argv:return
    c=root/'benchmarks/runs/20260908-5970-bve-active-pop-ranks1-40'
    old=root/'benchmarks/runs/20260908-5908-reduce-root-ranks1-40'
    target=c/'parent-work-validation.json';assert not target.exists()
    score=fixed_runtime.score(c);assert score['records']==score['expected_solves']==40
    rows=[];hashes={}
    for rank in range(1,41):
        paths=[base/'provisional'/f'{rank-1:03}.json' for base in (c,old)]
        a,b=[json.loads(p.read_text()) for p in paths]
        for key in ('instance','expected','verdict','expected_match','certificate_bytes','certificate_sha256'):
            assert a[key]==b[key],(rank,key)
        sa,ca=split(a['solver_statistics']);sb,cb=split(b['solver_statistics'])
        assert sa==sb and ca.keys()==cb.keys(),rank
        rows.append(dict(rank=rank,candidate_seconds=a['solve']['wall_seconds'],parent_seconds=b['solve']['wall_seconds'],
            changed_counters={k:dict(candidate=ca[k],parent=cb[k]) for k in ca if ca[k]!=cb[k]}))
        hashes.update({str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in paths})
    target.write_text(json.dumps(dict(all40_certificate_identity=True,all40_other_nontiming_statistics_identity=True,
        changed_ranks=[r['rank'] for r in rows if r['changed_counters']],rows=rows,hashes=hashes,
        limitation='Published non-timing statistics and complete certificate byte hashes agree except the two intended counters. Not a hidden whole-state claim or isolated unit-cost attribution. Full candidate1.116453x fixed reference does not improve the gap.'),indent=2)+'\n')
    print('PASS:40 complete certificate identities;only deactivate/bve counters differ on', [r['rank'] for r in rows if r['changed_counters']])

if __name__=='__main__':main()

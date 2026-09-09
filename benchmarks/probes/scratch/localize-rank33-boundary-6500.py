#!/usr/bin/env python3
"""Locate projected boundary differences after hash-bound complete decision replay."""
import itertools
import json
from pathlib import Path
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
c=root/'benchmarks/runs/20260909-6486-rank33-later-prefix'
target=c/'boundary-localization-6500.json'; assert not target.exists()
p=c/'summary.json'; s=json.loads(p.read_text())
assert s['pairs']['candidate_kissat']['matching_decisions_from_start']==55106721
assert s['pairs']['candidate_previous']['matching_decisions_from_start']==45379152
names={'congruence':'Congruence','substitute_early':'Substitute','substitute_late':'Substitute',
       'backbone_early':'Backbone','backbone_late':'Backbone','vivify':'Vivify','sweep':'Sweep',
       'transitive':'TransitiveReduce','factor':'Factor','strengthen':'Strengthen','bve':'BVE'}
hashes={str(p.relative_to(root)):sha(p)}; boundaries={}
for arm in ('candidate','previous','kissat'):
    p=c/'traces'/f'{arm}.boundaries.tsv'
    assert sha(p)==s['hashes'][str(p.relative_to(root))]
    hashes[str(p.relative_to(root))]=sha(p)
    rows=[]
    for line in p.read_text().splitlines():
        if line.startswith('#'): continue
        row=line.split('\t')
        if int(row[4])==0 or row[2] not in (names if arm=='kissat' else names.values()): continue
        rows.append(dict(name=names.get(row[2],row[2]),stage=row[3],conflicts=int(row[4]),
            decisions=int(row[5]),active=int(row[7]),irred=int(row[10]),red=int(row[11]),binary=int(row[12])))
    boundaries[arm]=rows
pairs={}
for a,b in itertools.combinations(boundaries,2):
    first=None
    for index,(x,y) in enumerate(itertools.zip_longest(boundaries[a],boundaries[b])):
        if x!=y:
            first=dict(index=index,left=x,right=y); break
    pairs[a+'_'+b]=dict(first=first,context=None if first is None else
        {arm:boundaries[arm][max(0,first['index']-2):first['index']+3] for arm in (a,b)})
result=dict(pairs=pairs,hashes=hashes,decision_pairs=s['pairs'],
    limitation='Projected nonzero-conflict rule boundaries only. Counts and matching decisions do not prove formula or ordering identity; first difference is a causal lead, not repair acceptance.')
target.write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(pairs,indent=2))

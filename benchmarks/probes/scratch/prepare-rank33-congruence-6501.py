#!/usr/bin/env python3
"""Neutral Congruence census at the first nonbinary projected Rank33 split."""
import itertools
import json
from pathlib import Path
import shutil
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
old=root/'benchmarks/runs/20260909-6486-rank33-later-prefix'
c=root/'benchmarks/runs/20260909-6501-rank33-congruence-replay'; assert not c.exists()
p=old/'summary.json'; s=json.loads(p.read_text())
assert s['pairs']['candidate_kissat']['matching_decisions_from_start']==55106721
assert s['pairs']['candidate_kissat']['left']==[227618,54966705,204,-233254,0]
for rel,digest in s['hashes'].items(): assert sha(root/rel)==digest,rel
names={'congruence':'Congruence','substitute_early':'Substitute','substitute_late':'Substitute',
       'backbone_early':'Backbone','backbone_late':'Backbone','vivify':'Vivify','sweep':'Sweep',
       'transitive':'TransitiveReduce','factor':'Factor','strengthen':'Strengthen','bve':'BVE'}
boundaries={}
for arm in ('candidate','kissat'):
    rows=[]
    for line in (old/'traces'/f'{arm}.boundaries.tsv').read_text().splitlines():
        if line.startswith('#'): continue
        row=line.split('\t')
        if int(row[4])==0 or row[2] not in (names if arm=='kissat' else names.values()): continue
        rows.append(dict(name=names.get(row[2],row[2]),stage=row[3],conflicts=int(row[4]),
            decisions=int(row[5]),active=int(row[7]),irred=int(row[10]),red=int(row[11]),binary=int(row[12])))
    boundaries[arm]=rows
first=None
for index,(a,b) in enumerate(itertools.zip_longest(boundaries['candidate'],boundaries['kissat'])):
    project=lambda r:None if r is None else {k:v for k,v in r.items() if k!='binary'}
    if project(a)!=project(b): first=dict(index=index,candidate=a,kissat=b); break
assert first['candidate']['name']==first['kissat']['name']=='Congruence'
assert first['candidate']['stage']==first['kissat']['stage']=='end'
assert first['candidate']['conflicts']==first['kissat']['conflicts']==227617
m=json.loads((old/'manifest.json').read_text())
for rel,digest in m['hashes'].items(): assert sha(old/rel)==digest,rel
for name in ('pins','logs','traces','results'): (c/name).mkdir(parents=True)
for arm in ('candidate','kissat'): shutil.copy2(old/'pins'/arm,c/'pins'/arm)
runner=(old/'run.py').read_text().replace('tactix-6486-','tactix-6501-')
runner=runner.replace("('candidate', 'previous', 'kissat')","('candidate', 'kissat')")
anchor="assert not any(key.endswith('_EVENT_TRACE') for key in env)"; assert runner.count(anchor)==1
extra="""for kind,suffix in (('BINARY','congruence-binaries'),('CANDIDATE','congruence-candidates')):
    env[prefix+'_CONGRUENCE_'+kind+'_TRACE']=str(c/'traces'/f'{arm}.{suffix}.tsv')
    env[prefix+'_CONGRUENCE_'+kind+'_TRACE_CONFLICT']=str(m['diagnostic_conflict'])
"""
runner=runner.replace(anchor,extra+anchor); compile(runner,str(c/'run.py'),'exec')
(c/'run.py').write_text(runner)
batch=(old/'array.sbatch').read_text().replace(str(old),str(c)).replace('6486-rank33-later-prefix','6501-rank33-congruence').replace('--array=0-2','--array=0-1')
assert '--array=0-1' in batch
(c/'array.sbatch').write_text(batch)
m.update(authorities={a:m['authorities'][a] for a in ('candidate','kissat')},
    parent_summary={'path':str(p.relative_to(root)),'sha256':sha(p)},
    parent_runs={a:str(old.relative_to(root)) for a in ('candidate','kissat')},
    diagnostic_conflict=227617,first_nonbinary_boundary=first,
    boundary_localization={'path':str((old/'boundary-localization-6500.json').relative_to(root)),
        'sha256':sha(old/'boundary-localization-6500.json')},
    purpose='Current6424/native first decision split227618 follows first nonbinary projected difference at Congruence end227617. Observe both invocations, entry/post-extraction/exit binaries and ternary candidates without assuming equal formulas or causal attribution.',
    validation='Require exact own6486 ordinary traces and partial-proof bytes/hash through1M before any census interpretation. Validate all new frames, clause identities and complete sequences. No broad event trace, early exit or reference timing refresh.',
    test_consideration='Existing immutable observers and bounded runner; seven new zero/one/multi threshold-overlap tests and full exact own replay. A new census parser must pass positive/negative framing fixtures before interpreting its results; no solver source changed.',
    limitation='Diagnostic localization only. Historical Sweep120330 root-satisfied equivalence evidence is retained, not assumed to prove current full-state equivalence. Partial proof cleanup is not final-proof acceptance.')
m['hashes']={str(q.relative_to(c)):sha(q) for q in [*(c/'pins').iterdir(),c/'run.py',c/'array.sbatch']}
(c/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
for q in (c/'pins').iterdir(): q.chmod(q.stat().st_mode & ~0o222)
print(c)

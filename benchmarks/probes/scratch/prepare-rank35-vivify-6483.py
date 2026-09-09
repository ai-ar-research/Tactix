#!/usr/bin/env python3
"""Observe both late Vivify invocations after exact first-decision localization."""
import csv
import json
from pathlib import Path
import shutil
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
old=root/'benchmarks/runs/20260909-6467-rank35-near-final-prefix'
c=root/'benchmarks/runs/20260909-6483-rank35-vivify-replay'; assert not c.exists()
summary=old/'summary.json'; s=json.loads(summary.read_text())
assert s['matching_decisions_from_start']==161899386
assert s['decisions']['left']==[3074183,161631566,1122,-482577,0]
assert s['decisions']['right']==[3074183,161631566,1122,-486307,0]
for rel,digest in s['hashes'].items(): assert sha(root/rel)==digest,rel
invocations={}
for arm in ('candidate','previous'):
    p=old/'traces'/f'{arm}.boundaries.tsv'
    with p.open() as stream:
        assert next(stream)=='# tactix-inprocess-boundary\t1\n'
        columns=next(stream).rstrip('\n').removeprefix('# ').split('\t')
        rows=list(csv.DictReader(stream,fieldnames=columns,delimiter='\t'))
    selected=[r for r in rows if r['name']=='Vivify' and int(r['conflicts'])==3069160]
    assert len(selected)==4
    invocations[arm]=[]
    for i in (0,2):
        a,b=selected[i:i+2]; assert a['stage']=='begin' and b['stage']=='end'
        assert a['decisions']==b['decisions']=='161392600'
        invocations[arm].append(dict(begin=a,end=b,
            propagations=int(b['propagations'])-int(a['propagations']),
            ticks=int(b['ticks'])-int(a['ticks'])))
for i in (0,1):
    a,b=invocations['candidate'][i],invocations['previous'][i]
    for stage in ('begin','end'):
        for key in ('conflicts','decisions','active','irred','red','binary'):
            assert a[stage][key]==b[stage][key]
    assert a['propagations']-b['propagations']==(0 if i==0 else 1)
    assert a['ticks']-b['ticks']==(0 if i==0 else 52)
m=json.loads((old/'manifest.json').read_text())
for rel,digest in m['hashes'].items(): assert sha(old/rel)==digest
for folder in ('pins','logs','results','traces'): (c/folder).mkdir(parents=True)
for p in (old/'pins').iterdir(): shutil.copy2(p,c/'pins'/p.name)
runner=(old/'run.py').read_text().replace('tactix-6467-','tactix-6483-')
anchor="assert not any(key.endswith('_EVENT_TRACE') for key in env)"; assert runner.count(anchor)==1
extra="""env['TACTIX_VIVIFY_CANDIDATE_TRACE'] = str(c/'traces'/f'{arm}.vivify-candidates.tsv')
env['SAT_VIVIFY_CANDIDATE_TRACE_CONFLICT'] = str(m['diagnostic_conflict'])
env['TACTIX_VIVIFY_ROUND_TRACE'] = str(c/'traces'/f'{arm}.vivify-rounds.tsv')
env['SAT_VIVIFY_ROUND_TRACE_CONFLICT'] = str(m['diagnostic_conflict'])
"""
runner=runner.replace(anchor,extra+anchor); compile(runner,str(c/'run.py'),'exec')
(c/'run.py').write_text(runner)
(c/'array.sbatch').write_text((old/'array.sbatch').read_text().replace(str(old),str(c)).replace('6467-rank35-near-final','6483-rank35-vivify'))
m.update(parent_summary={'path':str(summary.relative_to(root)),'sha256':sha(summary)},
    parent_runs={arm:str(old.relative_to(root)) for arm in ('candidate','previous')},
    diagnostic_conflict=3069160,selected_vivify_invocations=invocations,
    purpose='Rank35 first decision split3074183 follows equal-count Probe3069160; second Vivify has+1 propagation/+52ticks. Compare both invocations candidate order, clauses, poses, effort and budget using existing standalone observers.',
    validation='Before interpreting new Vivify rows, require every own6467 ordinary trace and partial-proof bytes/hash exactly through4.4M. Validate schema, complete rows, sequences and round framing. No global trace, policy change or native refresh.',
    test_consideration='Unchanged immutable solver/observer/runner paths plus new environment outputs. Existing complete-row/boundary controls cover parent replay. New Vivify reader fixtures cover schemas, truncation, sequence, selected conflict, clause multiset/pose and round framing before interpretation.',
    limitation='Selected late effort difference is a diagnostic lead, not cause or proof of formula identity. Full own replay is required; partial proof deletion is not final acceptance and instrumented time is not a speed claim.')
m['hashes']={str(p.relative_to(c)):sha(p) for p in [*(c/'pins').iterdir(),c/'run.py',c/'array.sbatch']}
(c/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
for p in (c/'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
print(c)

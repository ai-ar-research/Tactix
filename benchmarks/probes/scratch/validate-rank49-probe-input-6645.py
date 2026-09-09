#!/usr/bin/env python3
"""Validate complete first-Probe population and account its deadline input."""
from collections import Counter
import importlib.util
import json
import math
from pathlib import Path
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
from binary_snapshot import read_snapshots
from search_window import read_search
c=root/'benchmarks/runs/20260909-6643-rank49-probe-input'
target=c/'summary-6645.json';assert not target.exists()
arms=('candidate','kissat')
if any(not (c/f'results/{arm}.json').exists() for arm in arms):
    print('Probe snapshots pending; no summary written');sys.exit(0)
m=json.loads((c/'manifest.json').read_text());hashes={}
def bind(p,expected=None):
    digest=sha(p);assert expected is None or digest==expected,p
    hashes[str(p.relative_to(root))]=digest
for rel,digest in m['hashes'].items():bind(c/rel,digest)
for rel,digest in m['dependencies'].items():bind(root/rel,digest)
for name in ('manifest.json','launch.json'):bind(c/name)
parser=Path(__file__).with_name('compare-rank32-formula-5785.py')
spec=importlib.util.spec_from_file_location('formula5785',parser)
formula=importlib.util.module_from_spec(spec);spec.loader.exec_module(formula);formula.fixtures()
parent=root/m['cached_controls'];rows={};binaries={};deadlines={};boundary={}
for arm in arms:
    p=c/f'results/{arm}.json';bind(p)
    r=json.loads(p.read_text());old=json.loads((parent/f'results/{arm}.json').read_text())
    assert r['exit_code']==0 and r['scratch_deleted'] and r['arm']==arm
    assert r['binary_sha256']==old['binary_sha256']==m['hashes']['pins/'+arm]
    assert r['partial_certificate']==old['partial_certificate']
    assert set(r['trace_sha256'])-set(old['trace_sha256'])=={arm+'.formula.tsv',arm+'.binaries.tsv'}
    for name,digest in r['trace_sha256'].items():bind(c/'traces'/name,digest)
    for name,digest in old['trace_sha256'].items():assert r['trace_sha256'][name]==digest,name
    content=(c/f'traces/{arm}.formula.tsv').read_text();assert content.endswith('\n')
    rows[arm]=formula.read_rows(c/f'traces/{arm}.formula.tsv')
    with (c/f'traces/{arm}.binaries.tsv').open() as stream:
        snapshots=read_snapshots(stream,'tactix' if arm=='candidate' else 'kissat',2988,[('probe_end','-')])
    assert len(snapshots)==1;binaries[arm]=snapshots[0]['rows']
    search=read_search((c/f'traces/{arm}.search.tsv').read_text(),solver='tactix' if arm=='candidate' else 'kissat',
        window=tuple(m['search_window']),cap=m['trace_row_cap'])
    selected=[x for x in search if x['event']=='probe_end' and x['conflicts']==2988]
    assert len(selected)==1 and selected[0]['decisions']==15773 and selected[0]['level']==0
    boundary[arm]=selected[0]
    counts=Counter(row[0][0] for row in rows[arm])
    assert counts=={0:selected[0]['irred'],1:selected[0]['red']}
    assert len(binaries[arm])==selected[0]['binary']
    population=counts[0]+len(binaries[arm]);base=int(100*2*math.log10(2+9))
    scaled=int((4.5*(math.log10(population+10)-5)**2+25)*base)
    deadlines[arm]=dict(large_irredundant=counts[0],binary_copies=len(binaries[arm]),
        binary_unique=len(set(binaries[arm])),population=population,base=base,scaled_delta=scaled,
        computed_deadline=2988+scaled,first_conflict_eligible=2988+scaled+1)
def delta(a,b):
    return [dict(row=row,copies=n) for row,n in sorted((Counter(a)-Counter(b)).items())]
for p in (Path(__file__),parser,root/'tools/bench/binary_snapshot.py',
          root/'.claude/worktrees/runtime-material-schedule-6579/src/kernel/kernel.cpp',
          root/'.claude/worktrees/runtime-material-schedule-6579/src/policies/default_policy.cpp',
          root/'.claude/worktrees/runtime-material-schedule-6579/src/policies/default_strategy_config.hpp',
          root/'tools/solvers/variants/kissat-macro-heap-6090/src/kimits.c',
          root/'tools/solvers/variants/kissat-macro-heap-6090/src/kimits.h',
          root/'tools/solvers/variants/kissat-macro-heap-6090/src/probe.c'):bind(p)
out=dict(rank=49,full_own_replay=True,boundaries=boundary,deadline_accounting=deadlines,
    binary_unique_sets_equal=set(binaries['candidate'])==set(binaries['kissat']),
    candidate_only_binary=delta(binaries['candidate'],binaries['kissat']),
    kissat_only_binary=delta(binaries['kissat'],binaries['candidate']),
    large_key_multisets_equal=Counter(row[0] for row in rows['candidate'])==Counter(row[0] for row in rows['kissat']),
    full_large_rows_equal=rows['candidate']==rows['kissat'],
    candidate_only_large=delta(rows['candidate'],rows['kissat']),
    kissat_only_large=delta(rows['kissat'],rows['candidate']),hashes=hashes,
    limitation='Exact physical formula inputs and source-derived next deadline, not yet a producer defect or measured repair. Do not deduplicate policy counts independently of actual native representation. Large row differences retain pose/meta/root fields, not just semantic keys.')
with target.open('x') as stream:json.dump(out,stream,indent=2);stream.write('\n')
print(json.dumps({k:v for k,v in out.items() if k not in ('hashes','candidate_only_large','kissat_only_large','boundaries')},indent=2))
print('full large row differences',len(out['candidate_only_large']),len(out['kissat_only_large']))

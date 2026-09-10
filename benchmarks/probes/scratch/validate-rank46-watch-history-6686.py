#!/usr/bin/env python3
"""Validate complete replay before localizing selected clause order history."""
import importlib.util
import json
from pathlib import Path
import sys
root=Path(__file__).resolve().parents[3]; sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
from search_window import read_search,macro_replay
from watch_history import validate_projection,selected_blocks
c=root/'benchmarks/runs/20260909-6684-rank46-watch-history'
target=c/'summary-6686.json'; assert not target.exists()
if any(not (c/f'results/{a}.json').exists() for a in ('candidate','kissat')):
    print('Pending diagnostic; no summary'); sys.exit(0)
m=json.loads((c/'manifest.json').read_text()); hashes={}
def bind(p,expected=None):
    digest=sha(p); assert expected is None or digest==expected,p
    hashes[str(p.relative_to(root))]=digest
for rel,digest in m['hashes'].items(): bind(c/rel,digest)
for rel,digest in m['dependencies'].items(): bind(root/rel,digest)
for name in ('manifest.json','launch.json'): bind(c/name)
normalizer=Path(__file__).with_name('validate-square-phase-5486.py')
spec=importlib.util.spec_from_file_location('native_normalizer',normalizer)
native=importlib.util.module_from_spec(spec); spec.loader.exec_module(native); native.self_test()
keys={a:dict(p.split('=',1) for p in rr[0]['payload'].split('|'))['set'] for a,rr in m['conflict_records'].items()}
replay={}; blocks={}
for arm in ('candidate','kissat'):
    p=c/f'results/{arm}.json'; bind(p); r=json.loads(p.read_text())
    parent=root/m['cached_controls'][arm]; old=json.loads((parent/f'results/{arm}.json').read_text())
    standalone=root/m['standalone_controls'][arm]; ctrl=json.loads((standalone/f'results/{arm}.json').read_text())
    assert r['exit_code']==0 and r['scratch_deleted'] and r['arm']==arm
    assert r['binary_sha256']==old['binary_sha256']==m['hashes']['pins/'+arm]
    for key in ('partial_certificate','prior_certificate_prefix'): assert r[key]==old[key]==ctrl[key],key
    assert r['trace_sha256'].keys()==old['trace_sha256'].keys()
    for name,digest in r['trace_sha256'].items(): bind(c/'traces'/name,digest)
    info=dict(exact_traces=[],additions={})
    for name,digest in ctrl['trace_sha256'].items():
        if r['trace_sha256'][name]==digest: info['exact_traces'].append(name)
        elif name=='candidate.macro.tsv':
            extra=macro_replay((standalone/'traces'/name).read_text(),(c/'traces'/name).read_text())
            assert set(extra['extra_counts']) <= {'trace_begin','retier','checkpoint'}
            info['additions'][name]=extra
        elif name=='kissat.boundaries.tsv':
            def boundary_rows(p): return [l.split('\t') for l in p.read_text().splitlines() if not l.startswith('#')]
            count=native.validate_native_boundaries(boundary_rows(standalone/'traces'/name),boundary_rows(c/'traces'/name),tuple(m['search_window']))
            info['additions'][name]=dict(extra_analysis_end=count)
        else: raise AssertionError((name,'unexplained observer change'))
    for name in (arm+'.decisions.tsv',arm+'.prefix.tsv'): assert name in info['exact_traces']
    solver='tactix' if arm=='candidate' else 'kissat'
    before=read_search((parent/f'traces/{arm}.search.tsv').read_text(),solver=solver,window=(108330,108341),cap=250000)
    after=read_search((c/f'traces/{arm}.search.tsv').read_text(),solver=solver,window=tuple(m['search_window']),cap=m['trace_row_cap'])
    assert all(m['search_window'][0]<=row['conflicts']<=m['search_window'][1] for row in after)
    info['exact_old_watch_rows']=validate_projection(before,after,window=(108330,108341),scan=-16719)
    info['total_rows']=len(after); replay[arm]=info
    blocks[arm]=selected_blocks(after,scan=-16719,clause_keys=keys)
pair_observations={a:[dict(state=b['state'],first_seq=b['first_seq'],entries=b['entries'],
        positions={label:rr[0]['position'] for label,rr in b['hits'].items()},
        poses={label:rr[0]['fields']['pose'] for label,rr in b['hits'].items()}) for b in bb if b['unique_pair']] for a,bb in blocks.items()}
for p in (Path(__file__),normalizer,root/'tools/bench/watch_history.py',root/'tools/bench/test_watch_history.py',root/'tools/bench/watch_observer.py',root/'tools/bench/search_window.py'): bind(p)
out=dict(rank=46,stop=1000000,observer_replay_validated=True,replay=replay,
    selected_clause_keys=keys,selected_blocks=blocks,pair_observations=pair_observations,hashes=hashes,
    limitation='Contiguous equal-state watch blocks are not assumed unique scans. All selected occurrences retained; duplicate/one-sided blocks not coerced to unique pairs. The scan filter sees only-16719, so absent observations do not prove absent clauses or moves on other literals. No final proof or timing acceptance from this stopped run.')
with target.open('x') as f: json.dump(out,f,indent=2); f.write('\n')
print(json.dumps(dict(replay={a:{k:v for k,v in x.items() if k!='additions'} for a,x in replay.items()},
    blocks={a:len(x) for a,x in blocks.items()},pair_counts={a:len(x) for a,x in pair_observations.items()},
    first_pairs={a:x[:3] for a,x in pair_observations.items()}),indent=2))

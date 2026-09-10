#!/usr/bin/env python3
"""Validate all-scan projection and retain every selected clause observation."""
import importlib.util
import json
from pathlib import Path
import sys
root=Path(__file__).resolve().parents[3]; sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
from search_window import read_search,macro_replay
from multi_scan_watch import parse_watch,multi_scan_projection
c=root/'benchmarks/runs/20260909-6702-rank46-allscan-reason'
target=c/'summary-6704.json'; assert not target.exists()
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
replay={}; selected={}
for a in ('candidate','kissat'):
    p=c/f'results/{a}.json'; bind(p); r=json.loads(p.read_text())
    parent=root/m['cached_controls'][a]; old=json.loads((parent/f'results/{a}.json').read_text())
    standalone=root/m['standalone_controls'][a]; ctrl=json.loads((standalone/f'results/{a}.json').read_text())
    assert r['exit_code']==0 and r['scratch_deleted'] and r['arm']==a
    assert r['binary_sha256']==old['binary_sha256']==m['hashes']['pins/'+a]
    for key in ('partial_certificate','prior_certificate_prefix'): assert r[key]==old[key]==ctrl[key]
    assert r['trace_sha256'].keys()==old['trace_sha256'].keys()
    for name,digest in r['trace_sha256'].items(): bind(c/'traces'/name,digest)
    info=dict(exact_traces=[],additions={})
    for name,digest in ctrl['trace_sha256'].items():
        if r['trace_sha256'][name]==digest: info['exact_traces'].append(name)
        elif name=='candidate.macro.tsv':
            info['additions'][name]=macro_replay((standalone/'traces'/name).read_text(),(c/'traces'/name).read_text())
        elif name=='kissat.boundaries.tsv':
            def boundary_rows(p): return [l.split('\t') for l in p.read_text().splitlines() if not l.startswith('#')]
            count=native.validate_native_boundaries(boundary_rows(standalone/'traces'/name),boundary_rows(c/'traces'/name),tuple(m['search_window']))
            info['additions'][name]=dict(extra_analysis_end=count)
        else: raise AssertionError((name,'unexplained observer change'))
    for name in (a+'.decisions.tsv',a+'.prefix.tsv'): assert name in info['exact_traces']
    solver='tactix' if a=='candidate' else 'kissat'
    before=read_search((parent/f'traces/{a}.search.tsv').read_text(),solver=solver,window=(0,108170),cap=2000000)
    after=read_search((c/f'traces/{a}.search.tsv').read_text(),solver=solver,window=tuple(m['search_window']),cap=m['trace_row_cap'])
    info['exact_old_projection']=multi_scan_projection(before,after,window=tuple(m['search_window']),variable=19005,prior_scan=-16365)
    info['total_rows']=len(after); replay[a]=info
    selected[a]=[r for r in after if parse_watch(r)['clause']==m['reason_keys']['kissat']]
    assert selected[a]
for p in (Path(__file__),normalizer,root/'tools/bench/multi_scan_watch.py',root/'tools/bench/test_multi_scan_watch.py',root/'tools/bench/watch_observer.py',root/'tools/bench/search_window.py'): bind(p)
out=dict(rank=46,observer_replay_validated=True,replay=replay,reason_key=m['reason_keys']['kissat'],selected_clause_rows=selected,hashes=hashes,
    limitation='Rows include every selected-clause occurrence on every watched literal; seq positions are relative to the variable-filtered stream, not full watch-list offsets. First differences require portable state/pose interpretation. No timing or final-proof acceptance from this stopped diagnostic.')
with target.open('x') as f: json.dump(out,f,indent=2); f.write('\n')
print(json.dumps(dict(replay={a:{k:v for k,v in r.items() if k!='additions'} for a,r in replay.items()},selected_counts={a:len(rr) for a,rr in selected.items()}),indent=2))

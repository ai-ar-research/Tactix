#!/usr/bin/env python3
"""Validate native10K replay and localize the current Rank21 extended split."""
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
root=Path(__file__).resolve().parents[3];sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
from decision_prefix import read_prefix,validate_common,validate_stream_accounting,first_stream_difference
from certificate_prefix import hash_prefix
c=root/'benchmarks/runs/20260909-6751-rank21-long-prefix'
target=c/'summary-6753.json';assert not target.exists()
if any(not (c/f'results/{a}.json').exists() for a in ('candidate','kissat')):
    print('Rank21 diagnostic pending; no summary');sys.exit(0)
m=json.loads((c/'manifest.json').read_text());old=root/m['cached_controls'];hashes={}
def bind(p,expected=None):
    digest=sha(p);assert expected is None or digest==expected,p
    hashes[str(p.relative_to(root))]=digest
for rel,digest in m['hashes'].items():bind(c/rel,digest)
for rel,digest in m['dependencies'].items():bind(root/rel,digest)
for name in ('manifest.json','launch.json'):bind(c/name)
prefixes={};streams={};controls={}
for a in ('candidate','kissat'):
    p=c/f'results/{a}.json';bind(p);r=json.loads(p.read_text())
    assert r['exit_code']==0 and r['scratch_deleted'] and r['arm']==a
    assert r['binary_sha256']==m['hashes']['pins/'+a]
    assert r['partial_certificate']['bytes']>0
    for name,digest in r['trace_sha256'].items():bind(c/'traces'/name,digest)
    p=read_prefix(c/f'traces/{a}.prefix.tsv')
    assert p['conflict']==50000 and p['common']==m['common_decisions']
    stream=c/f'traces/{a}.decisions.tsv'
    count=int(subprocess.check_output(['wc','-l',str(stream)],text=True).split()[0])
    with stream.open('rb') as inp:
        first=list(map(int,inp.readline().split(b'\t')))
        inp.seek(-1,2);assert inp.read(1)==b'\n'
    last=list(map(int,subprocess.check_output(['tail','-n','1',str(stream)],text=True).split('\t')))
    validate_stream_accounting(p,first,last,count)
    prefixes[a]=p;streams[a]=stream
    controls[a]=dict(old10k_common_hash_matches=p['common_reached']==1 and p['common_hash']==m['native_control']['prefix']['hash'],stream_rows=count)
    if a=='kissat':
        validate_common(p,stop=50000,common=m['common_decisions'],digest=m['native_control']['prefix']['hash'])
        assert r['prior_certificate_prefix']==m['native_control']['certificate']
        prior=old/'traces/kissat.decisions.tsv'
        assert hash_prefix(stream,prior.stat().st_size)['sha256']==sha(prior)
        controls[a]['old_decisions_and_partial_proof_exact']=True
difference=first_stream_difference(streams['candidate'],streams['kissat'])
module_path=Path(__file__).with_name('localize-root-lock-rank37-6073.py')
spec=importlib.util.spec_from_file_location('rank21_boundaries',module_path)
bounds=importlib.util.module_from_spec(spec);spec.loader.exec_module(bounds);bounds.fixtures()
rows={};ignored={}
for a in ('candidate','kissat'):
    with (c/f'traces/{a}.boundaries.tsv').open() as inp:rows[a],ignored[a]=bounds.parse(inp,a=='kissat')
near={}
for a,key in (('candidate','left'),('kissat','right')):
    split=difference[key]
    if split is not None:near[a]=[r for r in rows[a] if r['conflicts']<=split[0]][-12:]
for p in (Path(__file__),module_path,root/'tools/bench/decision_prefix.py',root/'tools/bench/certificate_prefix.py'):bind(p)
out=dict(rank=21,stop=50000,bounded_stops_clean=True,prefixes=prefixes,controls=controls,decisions=difference,
    boundaries_all=bounds.compare(rows['candidate'],rows['kissat']),
    boundaries_search=bounds.compare([r for r in rows['candidate'] if r['conflicts']>0],[r for r in rows['kissat'] if r['conflicts']>0]),
    last_boundaries_before_difference=near,ignored_boundary_names=ignored,hashes=hashes,
    limitation='Native replays every old10K decision and partial-proof prefix. Current source is not assumed neutral to6107; old-prefix comparison is explicit. Bound stream hashes/counts/endpoints and requested stops validated. First differing choice and preceding counts bracket investigation, not a cause or proof of weaker formula. No full SAT/model or runtime acceptance claim.')
with target.open('x') as f:json.dump(out,f,indent=2);f.write('\n')
print(json.dumps({k:out[k] for k in ('bounded_stops_clean','controls','decisions','last_boundaries_before_difference')},indent=2))

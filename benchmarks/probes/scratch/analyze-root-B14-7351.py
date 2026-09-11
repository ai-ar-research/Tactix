#!/usr/bin/env python3
"""Freeze complete BC classification and first full-trace decision divergence."""
from collections import deque
import datetime as dt
import gzip
import hashlib
import importlib.util
from itertools import zip_longest
import json
from pathlib import Path
import re
import subprocess
import sys
root=Path(__file__).resolve().parents[3]; sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
target=root/'benchmarks/reports/2026-09-11-root-B14-analysis-7351.json'
assert not target.exists(),'one-shot writer'
def module(name,file):
    spec=importlib.util.spec_from_file_location(name,Path(__file__).with_name(file))
    result=importlib.util.module_from_spec(spec); spec.loader.exec_module(result); return result
storage=module('storage','root-storage-statistics-7350.py'); storage.fixtures()
fp=module('fp','occurrence-fingerprint-7256.py'); fp.fixtures()
hashes={}; observations={}
def bind(path,expected=None):
    digest=sha(path); assert expected is None or digest==expected,path
    hashes[str(path.relative_to(root))]=digest
def record(c,task):
    path=c/f'results/{task:03d}.json'; final=path.exists()
    if not final: path=c/f'provisional/{task:03d}.json'
    data=json.loads(path.read_text()); state=classify(data,final)
    assert data['task']==task and state['status'] in ('pending_validation','checked_SAT','checked_UNSAT'),(path,state)
    bind(path); return data,state
for stem in ('root-storage-BC-7347','root-collector-isolation-7348','root-decision-trace-7349'):
    helper=Path(__file__).with_name('check-submit-'+stem+'.py')
    result=subprocess.run([sys.executable,helper],text=True,capture_output=True)
    assert result.returncode==0,(helper,result.stdout,result.stderr)
    observations[stem]=result.stdout; bind(helper)
bc=root/'benchmarks/runs/20260911-7347-root-storage-BC'
rows=[]; states=[]
for task in range(0,66,2):
    a,sa=record(bc,task); b,sb=record(bc,task+1); states.extend([sa,sb])
    assert a['rank']==b['rank'] and a['instance']==b['instance']
    row=dict(rank=a['rank'],tier=a['tier'],instance=a['instance'],
        raw_exact=fp.raw_fingerprint(a)==fp.raw_fingerprint(b),**storage.compare(a,b),
        current_seconds=a['solve']['wall_seconds'],candidate_seconds=b['solve']['wall_seconds'],
        current_rss_kib=a['solve']['peak_rss_kb'],candidate_rss_kib=b['solve']['peak_rss_kb'],
        current_certificate=a['certificate_sha256'],candidate_certificate=b['certificate_sha256'])
    rows.append(row)
assert [r['rank'] for r in rows if not r['storage_only']]==[14], 'additional changed-work case requires diagnosis'
trace=root/'benchmarks/runs/20260911-7349-root-decision-trace'
data=[]; paths=[]; trace_states=[]
for task in range(2):
    r,state=record(trace,task); data.append(r); trace_states.append(state)
    # The immutable runner closes/compresses both traces before atomically
    # publishing its solver record. These are completed artifacts, even while
    # independent certificate checking continues; never inspect a growing log.
    path=trace/f'traces/{task:03d}-decisions.tsv.gz'; bind(path)
    if 'trace_artifacts' in r: bind(path,r['trace_artifacts']['decisions']['sha256'])
    paths.append(path)
counts=[0,0]; digests=[hashlib.sha256(),hashlib.sha256()]
first=None; preceding=deque(maxlen=5)
with gzip.open(paths[0],'rb') as a,gzip.open(paths[1],'rb') as b:
    for ordinal,pair in enumerate(zip_longest(a,b)):
        parsed=[]
        for arm,line in enumerate(pair):
            if line is None: parsed.append(None); continue
            digests[arm].update(line); row=tuple(map(int,line.split())); assert len(row)==5
            counts[arm]+=1
            assert row[1]==counts[arm] and row[0]>=0 and row[2]>=0 and row[3]!=0 and row[4] in (0,1),row
            parsed.append(row)
        if first is None:
            if parsed[0]!=parsed[1]:
                first=dict(ordinal=ordinal+1,identical_preceding_decisions=ordinal,
                    current=parsed[0],candidate=parsed[1],preceding=list(preceding))
            else: preceding.append(parsed[0])
assert first and first['current'] and first['candidate']
for arm,r in enumerate(data):
    expected=[re.fullmatch(r'c decisions   (\d+) \(0 via API\)',s) for s in r['solver_statistics'] if s.startswith('c decisions')]
    assert len(expected)==1 and expected[0] and counts[arm]==int(expected[0][1]),(counts,expected)
    if 'trace_artifacts' in r: assert digests[arm].hexdigest()==r['trace_artifacts']['decisions']['uncompressed_sha256']
macro=[]
for task in range(2):
    path=trace/f'traces/{task:03d}-macro.tsv.gz'; bind(path)
    if 'trace_artifacts' in data[task]: bind(path,data[task]['trace_artifacts']['macro']['sha256'])
    with gzip.open(path,'rt') as stream:
        entries=[line.rstrip('\n').split('\t') for line in stream if not line.startswith('#')]
    assert entries and all(len(row)==15 for row in entries)
    macro.append(entries)
portable=(2,3,4,5,6,7,8,10,11,12,13)
first_macro=None
for ordinal,(a,b) in enumerate(zip_longest(*macro)):
    if a is None or b is None or tuple(a[i] for i in portable)!=tuple(b[i] for i in portable):
        first_macro=dict(ordinal=ordinal+1,current=a,candidate=b,
            preceding_current=macro[0][max(0,ordinal-4):ordinal],preceding_candidate=macro[1][max(0,ordinal-4):ordinal]); break
near=[]
for arm,entries in enumerate(macro):
    choice=first['current' if arm==0 else 'candidate']
    before=[row for row in entries if int(row[4])<=choice[1]]
    after=[row for row in entries if int(row[4])>choice[1]]
    near.append(dict(before=before[-6:],after=after[:2]))
for path in [Path(__file__),Path(__file__).with_name('root-storage-statistics-7350.py'),
             Path(__file__).with_name('occurrence-fingerprint-7256.py')]: bind(path)
for c in (bc,trace,root/'benchmarks/runs/20260911-7348-root-collector-isolation'):
    bind(c/'manifest.json'); bind(c/'launch.json')
summary=dict(status='REGRESSION_LOCALIZED_NOT_ACCEPTED',created_at=dt.datetime.now(dt.timezone.utc).isoformat(),
    hashes=hashes,reader_observations=observations,bc_rows=rows,bc_checked=sum(s['checked'] for s in states),
    bc_raw_exact=sum(r['raw_exact'] for r in rows),bc_storage_only=sum(r['storage_only'] for r in rows),
    decisions=dict(counts=counts,uncompressed_sha256=[h.hexdigest() for h in digests],first_difference=first),
    first_portable_macro_difference=first_macro,macro_near_first_decision=near,trace_states=trace_states,
    decision='B14 blocks root-storage performance acceptance. Actual7335 and7340 full work/certificates agree in isolation7348, so collector correction is not its cause. Other32 BC pairs have exact certificates and non-storage counters with conserved physical+removed slots. No new Rank25/31 waves or full50 until migration drift is explained.',
    limitation='First trace difference localizes manifestation, not its root cause. Raw trail/stamp and state digests are representation-sensitive. Pending original-proof checks permit provisional analysis, not correctness acceptance. Timings from traced or one-wave diagnostic runs are not decision-grade performance evidence.')
with target.open('x') as out: json.dump(summary,out,indent=2)
print(json.dumps(dict(target=str(target.relative_to(root)),bc_checked=summary['bc_checked'],
    counts=counts,first_difference=first,first_macro=first_macro,near=near),indent=2))

#!/usr/bin/env python3
"""Classify the exact Eliminate active-set/slot difference after whole replay."""
import gzip
import importlib.util
from itertools import zip_longest
import json
from pathlib import Path
import subprocess
import sys
root=Path(__file__).resolve().parents[3];sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
def active_set(text):
    lines=text.splitlines()
    assert lines[0]=='# tactix-macro-active-set\t1' and lines[1]=='# solver\tevent\tconflicts\tactive'
    header=lines[2].split('\t'); assert len(header)==4 and header[:3]==['# tactix','eliminate_end','26295']
    assert lines[3]=='# external'
    values=list(map(int,lines[4:])); assert all(v>0 for v in values)
    assert len(values)==len(set(values))==int(header[3])
    return set(values)
sample='# tactix-macro-active-set\t1\n# solver\tevent\tconflicts\tactive\n# tactix\teliminate_end\t26295\t2\n# external\n1\n3\n'
assert active_set(sample)=={1,3}
for invalid in [sample.replace('26295','26296'),sample.replace('\n3\n','\n1\n'),sample.replace('\t2\n','\t3\n')]:
    try: active_set(invalid)
    except AssertionError: pass
    else: raise AssertionError('invalid active snapshot accepted')
if sys.argv[1:]==['--self-test']: print('active-set parser: exact conflict/count/unique identity controls PASS');sys.exit(0)
assert not sys.argv[1:]
c=root/'benchmarks/runs/20260911-7353-root-phase-trace'
old=root/'benchmarks/runs/20260911-7349-root-decision-trace'
target=root/'benchmarks/reports/2026-09-11-root-phase-analysis-7354.json'
assert not target.exists()
helper=Path(__file__).with_name('check-submit-root-phase-trace-7353.py')
checked=subprocess.run([sys.executable,helper],text=True,capture_output=True)
assert checked.returncode==0,(checked.stdout,checked.stderr)
hashes={}; sets=[]; boundaries=[]; states=[]
def bind(path,expected=None):
    digest=sha(path);assert expected is None or digest==expected,path
    hashes[str(path.relative_to(root))]=digest
for task in range(2):
    path=c/f'results/{task:03d}.json';final=path.exists()
    if not final:path=c/f'provisional/{task:03d}.json'
    record=json.loads(path.read_text());state=classify(record,final)
    assert record['task']==task and state['status'] in ('pending_validation','checked_UNSAT');states.append(state);bind(path)
    old_record_path=old/f'results/{task:03d}.json'
    old_record=json.loads(old_record_path.read_text());assert classify(old_record,True)['checked'];bind(old_record_path)
    for kind in ('decisions','macro','active','inprocess'):
        path=c/f'traces/{task:03d}-{kind}.tsv.gz';bind(path)
        if 'trace_artifacts' in record:bind(path,record['trace_artifacts'][kind]['sha256'])
        # Entire compressed streams (deterministic gzip) must replay7349, not
        # merely the selected conflict. Both sources and full proofs unchanged.
        if kind in ('decisions','macro'):
            prior=old_record['trace_artifacts'][kind];bind(old/prior['path'],prior['sha256']);bind(path,prior['sha256'])
        if kind=='active':
            with gzip.open(path,'rt') as stream:sets.append(active_set(stream.read()))
        if kind=='inprocess':
            with gzip.open(path,'rt') as stream:rows=[line.rstrip('\n').split('\t') for line in stream if not line.startswith('#')]
            assert rows and all(len(row)==15 and int(row[4])>=26295 for row in rows)
            boundaries.append(rows)
portable=(2,3,4,5,6,7,8,10,11,12,13,14)
first=None
for ordinal,(a,b) in enumerate(zip_longest(*boundaries)):
    if a is None or b is None or tuple(a[i] for i in portable)!=tuple(b[i] for i in portable):
        first=dict(ordinal=ordinal+1,current=a,candidate=b);break
first_phase=[[row for row in rows if int(row[4])==26295] for rows in boundaries]
bind(Path(__file__));bind(helper);bind(c/'manifest.json');bind(c/'launch.json')
summary=dict(status='WHOLE_REPLAY_AND_SELECTED_STATE_VALIDATED',hashes=hashes,states=states,
    active_counts=[len(s) for s in sets],current_only=sorted(sets[0]-sets[1]),candidate_only=sorted(sets[1]-sets[0]),
    first_portable_boundary_difference=first,selected_phase_boundaries=first_phase,
    reader_output=checked.stdout,
    limitation='Identifies active identities and first exposed phase difference, not all earlier latent heap/order/state causes. Original proof checking can remain pending; no performance or promotion claim.')
with target.open('x') as out:json.dump(summary,out,indent=2)
print(json.dumps({key:value for key,value in summary.items() if key not in ('hashes','reader_output')},indent=2))

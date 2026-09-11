#!/usr/bin/env python3
"""Fail-closed three-pin diagnostic reader; optional once-only submission."""
import csv
import datetime as dt
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
def identity(record,row):
    assert record['task']==int(row['task']) and record['rank']==int(row['rank'])==14
    for key in ('arm','instance','expected'): assert record[key]==row[key]
def fixtures():
    row=dict(task='0',rank='14',arm='parent',instance='x',expected='UNSAT')
    record=dict(row,task=0,rank=14); identity(record,row)
    for key,value in [('task',1),('rank',25),('arm','both'),('instance','y'),('expected','SAT')]:
        try: identity(dict(record,**{key:value}),row)
        except AssertionError: pass
        else: raise AssertionError(key)
assert sys.argv[1:] in ([],['--submit'],['--self-test']); fixtures()
if sys.argv[1:]==['--self-test']: print('isolation identity: positive and five negative controls PASS'); sys.exit(0)
c=root/'benchmarks/runs/20260911-7348-root-collector-isolation'
m=json.loads((c/'manifest.json').read_text())
for rel,digest in m['pin_sha256'].items(): assert sha(c/rel)==digest,rel
for rel,digest in m['dependencies'].items(): assert sha(root/rel)==digest,rel
spec=importlib.util.spec_from_file_location('fp',Path(__file__).with_name('occurrence-fingerprint-7256.py'))
h=importlib.util.module_from_spec(spec); spec.loader.exec_module(h); h.fixtures()
with (c/'matrix.tsv').open() as stream: rows=list(csv.DictReader(stream,delimiter='\t'))
assert [(int(r['task']),r['arm']) for r in rows]==[(0,'parent'),(1,'root_only'),(2,'both')]
records,states={},{}
for folder in ('provisional','results'):
    for path in sorted((c/folder).glob('*.json')):
        r=json.loads(path.read_text()); task=r['task']; assert task==int(path.stem) and task in range(3)
        identity(r,rows[task]); state=classify(r,folder=='results')
        assert state['status'] in ('pending_validation','checked_SAT','checked_UNSAT'),(path,state)
        if folder=='results': assert r['binary_sha256']==m['arms'][r['arm']]['sha256']
        if task in records: assert h.raw_fingerprint(r)==h.raw_fingerprint(records[task]) and r['solve']==records[task]['solve']
        if r['arm']!='root_only':
            key='current_B' if r['arm']=='parent' else 'candidate_B'
            oracle=json.loads((root/m['previous_work_oracles'][key]).read_text())
            assert h.raw_fingerprint(r)==h.raw_fingerprint(oracle),('own replay failed',task)
        records[task],states[task]=r,state
print(json.dumps(dict(records=len(records),checked=sum(s['checked'] for s in states.values()),
    observations=[dict(arm=r['arm'],time=r['solve']['wall_seconds'],rss=r['solve']['peak_rss_kb'],
        certificate=r['certificate_sha256']) for r in records.values()],
    root_only_equals_both=(h.raw_fingerprint(records[1])==h.raw_fingerprint(records[2])) if 1 in records and 2 in records else None),indent=2))
if not sys.argv[1:]: sys.exit(0)
assert not records and not (c/'launch.json').exists()
since=dt.datetime.now().replace(microsecond=0).isoformat(); command=['sbatch','--parsable',str(c/'array.sbatch')]
job=subprocess.check_output(command,text=True).strip().split(';')[0]; assert job.isdigit()
with (c/'launch.json').open('x') as out: json.dump(dict(job_id=job,accounting_since=since,command=command,
    manifest_sha256=sha(c/'manifest.json'),helper_sha256=sha(Path(__file__))),out,indent=2)
print(job,since)

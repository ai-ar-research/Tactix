#!/usr/bin/env python3
"""Exact own folded replay and predeclared cross-arm storage-only test."""
import csv
import datetime as dt
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
root=Path(__file__).resolve().parents[3];sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
assert sys.argv[1:] in ([],['--submit'])
c=root/'benchmarks/runs/20260911-7380-corrected-root-pair';m=json.loads((c/'manifest.json').read_text())
for rel,digest in m['pin_sha256'].items():assert sha(c/rel)==digest
for rel,digest in m['dependencies'].items():assert sha(root/rel)==digest
def module(name,file):
    spec=importlib.util.spec_from_file_location(name,Path(__file__).with_name(file));h=importlib.util.module_from_spec(spec);spec.loader.exec_module(h);h.fixtures();return h
fp=module('fp','occurrence-fingerprint-7256.py');storage=module('storage','root-storage-statistics-7350.py')
with (c/'matrix.tsv').open() as stream:rows=list(csv.DictReader(stream,delimiter='\t'))
assert [(int(r['task']),r['arm']) for r in rows]==[(0,'legacy'),(1,'folded')]
records={};states={}
for folder in ('provisional','results'):
    for path in (c/folder).glob('*.json'):
        r=json.loads(path.read_text());t=r['task'];assert t==int(path.stem) and t in (0,1)
        assert r['rank']==int(rows[t]['rank'])==14
        for key in ('arm','instance','expected'):assert r[key]==rows[t][key]
        state=classify(r,folder=='results');assert state['status'] in ('pending_validation','checked_UNSAT')
        if folder=='results':assert r['binary_sha256']==m['arms'][r['arm']]['sha256']
        if t in records:assert fp.raw_fingerprint(r)==fp.raw_fingerprint(records[t]) and r['solve']==records[t]['solve']
        if t==1:assert fp.raw_fingerprint(r)==fp.raw_fingerprint(json.loads((root/m['folded_oracle']).read_text()))
        records[t]=r;states[t]=state
comparison=storage.compare(records[0],records[1]) if len(records)==2 else None
print(json.dumps(dict(records=len(records),checked=sum(s['checked'] for s in states.values()),states=states,comparison=comparison,
    observations=[dict(task=t,solve=r['solve'],certificate_sha256=r['certificate_sha256'],statistics=r['solver_statistics']) for t,r in records.items()]),indent=2))
if not sys.argv[1:]:sys.exit(0)
assert not records and not (c/'launch.json').exists()
since=dt.datetime.now().replace(microsecond=0).isoformat();command=['sbatch','--parsable',str(c/'array.sbatch')]
job=subprocess.check_output(command,text=True).strip().split(';')[0];assert job.isdigit()
with (c/'launch.json').open('x') as out:json.dump(dict(job_id=job,accounting_since=since,command=command,manifest_sha256=sha(c/'manifest.json'),helper_sha256=sha(Path(__file__))),out,indent=2)
print(job,since)

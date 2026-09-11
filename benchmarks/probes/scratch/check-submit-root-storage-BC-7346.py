#!/usr/bin/env python3
"""Read-only all-field BC comparison; optional once-only full-width submit."""
import csv
import datetime as dt
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
c = root / 'benchmarks/runs/20260911-7346-root-storage-BC'
spec = importlib.util.spec_from_file_location('raw_fingerprint',Path(__file__).with_name('occurrence-fingerprint-7256.py'))
h = importlib.util.module_from_spec(spec); spec.loader.exec_module(h); h.fixtures()
assert sys.argv[1:] in ([],['--submit'])
m = json.loads((c/'manifest.json').read_text())
for rel,digest in m['pin_sha256'].items(): assert sha(c/rel)==digest,rel
for rel,digest in m['dependencies'].items(): assert sha(root/rel)==digest,rel
with (c/'matrix.tsv').open() as stream: matrix=list(csv.DictReader(stream,delimiter='\t'))
assert len(matrix)==66 and [int(r['task']) for r in matrix]==list(range(66))
records,states={},{}
for folder in ('provisional','results'):
    for path in sorted((c/folder).glob('*.json')):
        r=json.loads(path.read_text()); task=r['task']
        assert task==int(path.stem) and task in range(66)
        for key in ('arm','expected','instance'): assert r[key]==matrix[task][key]
        assert r['rank']==int(matrix[task]['rank'])
        state=classify(r,folder=='results')
        assert state['status'] in ('pending_validation','checked_SAT','checked_UNSAT'),(path,state)
        if folder=='results': assert r['binary_sha256']==m['arms'][r['arm']]['sha256']
        if task in records:
            assert h.raw_fingerprint(r)==h.raw_fingerprint(records[task]) and r['solve']==records[task]['solve']
        records[task],states[task]=r,state
exact=0; differences=[]
for task in range(0,66,2):
    if task not in records or task+1 not in records: continue
    a,b=records[task],records[task+1]
    if h.raw_fingerprint(a)==h.raw_fingerprint(b): exact+=1; continue
    delta={key:[a[key],b[key]] for key in ('certificate_sha256','certificate_bytes','verdict') if a[key]!=b[key]}
    al=[s for s in a['solver_statistics'] if not s.startswith('c timing')]
    bl=[s for s in b['solver_statistics'] if not s.startswith('c timing')]
    if al!=bl:
        delta['statistics']={'current_only':[s for s in al if s not in bl],
                             'candidate_only':[s for s in bl if s not in al],
                             'current_full':al,'candidate_full':bl}
    differences.append(dict(rank=a['rank'],instance=a['instance'],differences=delta))
print(json.dumps(dict(records=len(records),checked_clean=sum(s['checked'] for s in states.values()),
    exact_pairs=exact,unclassified_differences=differences,
    status='PENDING' if len(records)<66 else ('CLASSIFICATION_REQUIRED' if differences else 'EXACT_SOLVER_GATE')),
    indent=2))
if not sys.argv[1:]: sys.exit(0)
assert not records and not (c/'launch.json').exists(),'Already launched; monitor existing job'
since=dt.datetime.now().replace(microsecond=0).isoformat()
command=['sbatch','--parsable',str(c/'array.sbatch')]
job=subprocess.check_output(command,text=True).strip().split(';')[0]; assert job.isdigit()
with (c/'launch.json').open('x') as out:
    json.dump(dict(job_id=job,accounting_since=since,command=command,
        manifest_sha256=sha(c/'manifest.json'),helper_sha256=sha(Path(__file__))),out,indent=2)
print(job,since)

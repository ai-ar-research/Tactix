#!/usr/bin/env python3
"""Read published full trace solves and require exact own Release replay."""
import csv
import datetime as dt
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
root=Path(__file__).resolve().parents[3]; sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
assert sys.argv[1:] in ([],['--submit'])
c=root/'benchmarks/runs/20260911-7349-root-decision-trace'
m=json.loads((c/'manifest.json').read_text())
for rel,digest in m['pin_sha256'].items(): assert sha(c/rel)==digest,rel
for rel,digest in m['dependencies'].items(): assert sha(root/rel)==digest,rel
spec=importlib.util.spec_from_file_location('fp',Path(__file__).with_name('occurrence-fingerprint-7256.py'))
h=importlib.util.module_from_spec(spec); spec.loader.exec_module(h); h.fixtures()
with (c/'matrix.tsv').open() as stream: rows=list(csv.DictReader(stream,delimiter='\t'))
assert [(int(r['task']),r['arm']) for r in rows]==[(0,'parent'),(1,'candidate')]
records,states={},{}
for folder in ('provisional','results'):
    for path in sorted((c/folder).glob('*.json')):
        r=json.loads(path.read_text()); task=r['task']; assert task==int(path.stem) and task in range(2)
        assert r['rank']==int(rows[task]['rank'])==14
        for key in ('arm','expected','instance'): assert r[key]==rows[task][key]
        state=classify(r,folder=='results'); assert state['status'] in ('pending_validation','checked_UNSAT'),(path,state)
        if folder=='results': assert r['binary_sha256']==m['arms'][r['arm']]['sha256']
        if task in records: assert h.raw_fingerprint(r)==h.raw_fingerprint(records[task]) and r['solve']==records[task]['solve']
        oracle=json.loads((root/m['previous_work_oracles'][r['arm']]).read_text())
        assert h.raw_fingerprint(r)==h.raw_fingerprint(oracle),('diagnostic own replay failed',task)
        # The frozen runner's provisional schema may omit added trace metadata.
        # Final records retain it; analysis can hash only final-bound archives.
        if folder=='results':
            assert set(r['trace_artifacts'])=={'decisions','macro'}
            for artifact in r['trace_artifacts'].values(): assert sha(c/artifact['path'])==artifact['sha256']
        records[task],states[task]=r,state
print(json.dumps(dict(records=len(records),checked=sum(s['checked'] for s in states.values()),
    own_release_replay=True,states=states),indent=2))
if not sys.argv[1:]: sys.exit(0)
assert not records and not (c/'launch.json').exists()
since=dt.datetime.now().replace(microsecond=0).isoformat(); command=['sbatch','--parsable',str(c/'array.sbatch')]
job=subprocess.check_output(command,text=True).strip().split(';')[0]; assert job.isdigit()
with (c/'launch.json').open('x') as out: json.dump(dict(job_id=job,accounting_since=since,command=command,
    manifest_sha256=sha(c/'manifest.json'),helper_sha256=sha(Path(__file__))),out,indent=2)
print(job,since)

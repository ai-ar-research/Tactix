#!/usr/bin/env python3
"""Record the diagnostic that completed after the main checkpoint snapshot."""
import datetime
import hashlib
import json
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[3]
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def run(command): return subprocess.run(command,cwd=root,check=True,capture_output=True,text=True).stdout
c = root/'benchmarks/runs/20260907-5449-rank37-eliminate-limit'
m = json.loads((c/'manifest.json').read_text())
for rel,digest in m['hashes'].items(): assert sha(c/rel)==digest
previous = root/'benchmarks/runs/20260907-5436-rank37-inprocess-prefix'
summary = json.loads((previous/'summary.json').read_text())
assert summary['clean_stops']==2 and summary['matching_rows']==7504348
for arm in ('candidate','kissat'):
    r=json.loads((previous/'results'/(arm+'.json')).read_text())
    assert r['exit_code']==0 and r['scratch_deleted']
    for name,digest in r['trace_sha256'].items(): assert sha(previous/'traces'/name)==digest
result=dict(observed_at=datetime.datetime.now(datetime.timezone.utc).isoformat(),
    queue=run(['squeue','-j','1670217','-h','-o','%i %T %j']),
    state=run(['sacct','-j','1670217','-X','--format=JobID,State,ExitCode','--parsable2']),
    manifest_sha256=sha(c/'manifest.json'),
    prefix_summary_sha256=sha(previous/'summary.json'),
    boundary_localization_sha256=sha(previous/'boundary-localization.json'),
    progress_sha256=sha(root/'PROGRESS.md'),
    doc_checks={name:run(['python3','tools/docs/'+name+'.py']) for name in
        ('check-doc-size','check-section-refs','check-option-coverage','check-rule-policy-boundary')},
    limitation='Late diagnostic result after the main 22:07 validation snapshot; no proof/model failure, source change, fuzz inspection or deletion. Candidate-only ledger probe 5449 is pending, not a confirmed cause.')
p=root/'benchmarks/reports/2026-09-07-rank37-late-validation.json'
assert not p.exists()
p.write_text(json.dumps(result,indent=2)+'\n')
print('PASS: clean stopped-prefix evidence, new immutable diagnostic and updated living docs')

#!/usr/bin/env python3
"""Freeze current publication, pin, accounting and owned-artifact authority."""
import datetime as dt
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0,str(ROOT/'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify

TARGET = ROOT/'benchmarks/reports/2026-09-11-current-gaps-checkpoint-7400.json'
assert not TARGET.exists(), 'successful evidence frozen'
hashes = {}; owned = set()
def bind(p, expected=None, own=False):
    digest = sha(p); assert expected is None or digest == expected, str(p)
    rel = str(p.relative_to(ROOT)); hashes[rel] = digest
    if own: owned.add(rel)
def read(p, own=False):
    bind(p,own=own); return json.loads(p.read_text())

old = read(ROOT/'benchmarks/reports/2026-09-11-direct-fuzz-final-7395.json',True)
assert old['status'] == 'PASS'
for rel in old['owned_paths']: bind(ROOT/rel,old['hashes'][rel],True)
campaigns = {}; jobs = []
for stem, expected in [('7385-corrected-root-ranks1-50',50),('7382-corrected-root-runtime',12),
                       ('7389-current-gap-profiles',4),('7397-current-rank43-decisions',2)]:
    c = ROOT/('benchmarks/runs/20260911-'+stem)
    m = read(c/'manifest.json',True); states = {}
    for rel, digest in m['pin_sha256'].items():
        p = c/rel; bind(p,digest,p.suffix in ('.py','.patch','.tsv','.sbatch'))
    for p in c.glob('launch*.json'):
        launch = read(p,True); jobs.append((str(launch['job_id']),launch['accounting_since']))
    for folder in ('provisional','results'):
        for p in (c/folder).glob('*.json'):
            r = read(p,True); state = classify(r,folder=='results')
            assert state['status'] in ('pending_validation','checked_SAT','checked_UNSAT'),(str(p),state)
            states[r['task']] = state
    campaigns[stem] = dict(expected=expected,published=len(states),checked=sum(s['checked'] for s in states.values()),states=states)
for rel in ('benchmarks/runs/20260911-7389-current-gap-profiles/solver-summary-7390.json',
            'benchmarks/reports/2026-09-11-current-rank31-cost-7396.json',
            'benchmarks/reports/2026-09-11-direct-fuzz-complete-7395.md'):
    bind(ROOT/rel,own=True)
trace = ROOT/'benchmarks/runs/20260911-7397-current-rank43-decisions/solver-summary-7398.json'
if trace.exists():
    summary = read(trace,True)
    for rel, digest in summary['hashes'].items():
        bind(ROOT/rel,digest,rel.startswith('benchmarks/runs/20260911-7397-current-rank43-decisions/traces/'))
for name in ('current-rank31-cost-7396.py','current-rank43-decisions-7397.py','validate-current-rank43-7398.py','checkpoint-current-gaps-7400.py'):
    bind(Path(__file__).with_name(name),own=True)
accounting = {}
for job,since in sorted(set(jobs)):
    r = subprocess.run(['sacct','-n','-X','-j',job,'-S',since,'-o','JobID,State%30,ExitCode,Elapsed,AllocCPUS','-P'],text=True,capture_output=True,check=True)
    accounting[job] = dict(since=since,stdout=r.stdout)
    assert r.stdout.strip(), 'missing accounting is not terminal'
queue = subprocess.run(['squeue','-h','-r','-j',','.join(sorted({j for j,s in jobs})),'-o','%i|%T|%M|%R'],text=True,capture_output=True,check=True)
debug = subprocess.run(['ps','-p','3043191','-o','pid,ppid,etime,time,pcpu,stat,comm'],text=True,capture_output=True)
out = dict(observed_at=dt.datetime.now(dt.timezone.utc).isoformat(),hashes=hashes,owned_paths=sorted(owned),
    campaigns=campaigns,accounting=accounting,queue=queue.stdout,
    debug7342=dict(command=debug.args,exit=debug.returncode,stdout=debug.stdout,
                   complete_manifest_present=(ROOT/'benchmarks/runs/20260911-7342-debug-contracts-debug/manifest.json').exists()),
    manual_removed_bytes=0,
    limitations='Only published solver/final JSON and already terminal fuzz logs are hashed. Pending checks are not accepted; unchanged live scratch belongs to existing checker wrappers. No solver/source/pin or fixed denominator changed, no campaign restarted, no denied transfer or push retried. Fuzz gate passed but production consolidation, all current proof cleanup, runtime goal and synchronization remain open.')
with TARGET.open('x') as stream: json.dump(out,stream,indent=2)
print(TARGET)
print({k:(v['published'],v['checked'],v['expected']) for k,v in campaigns.items()})

#!/usr/bin/env python3
"""Seal confirmed timing, complete solver view and the replay-validated diagnosis."""
import datetime as dt
import json
from pathlib import Path
import re
import subprocess
import sys
root=Path(__file__).resolve().parents[3]; sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
target=root/'benchmarks/reports/2026-09-09-congruence-runtime-rank46-audit-6679.json'
assert not target.exists()
prior_path=root/'benchmarks/reports/2026-09-09-congruence-empty-gates-audit-6646.json'
prior=json.loads(prior_path.read_text())
for group in ('owned_hashes','protected_campaign_hashes','andrew_report'):
    for rel,digest in prior[group].items():
        if rel!='PROGRESS.md': assert sha(root/rel)==digest,rel
assert not subprocess.check_output(['git','diff','--','src','include','CMakeLists.txt'],cwd=root)
owned={'PROGRESS.md','tools/bench/certificate_prefix.py','tools/bench/test_certificate_prefix.py',
       'tools/bench/watch_observer.py','tools/bench/test_watch_observer.py',
       'benchmarks/reports/2026-09-09-congruence-runtime-and-rank46.md'}
for p in (root/'benchmarks/probes/scratch').iterdir():
    match=re.search(r'-(\d+)\.py$',p.name)
    if match and 6668<=int(match[1])<=6683: owned.add(str(p.relative_to(root)))
def own(p): owned.add(str(p.relative_to(root)))
def bindings(hashes):
    for rel,digest in hashes.items(): assert sha(root/rel)==digest,rel
accounting={}; evidence={}; builds={}
def account(c,launch,count,terminal=False):
    assert launch['manifest_sha256']==sha(c/'manifest.json')
    cmd=['sacct','-n','-X','--array','-j',launch['job_id'],'-S',launch['accounting_since'],
         '-o','JobID,State%40,ExitCode,Submit','-P']
    stdout=subprocess.check_output(cmd,text=True)
    rows=stdout.splitlines(); assert len(rows)==count,(c,stdout)
    for row in rows:
        cells=row.split('|')
        assert cells[1] in ('PENDING','RUNNING','COMPLETING','COMPLETED') and cells[2]=='0:0',row
        assert cells[3]>=launch['accounting_since']
        if terminal: assert cells[1]=='COMPLETED'
    accounting[launch['job_id']]=dict(campaign=str(c.relative_to(root)),command=cmd,stdout=stdout)
for name,summary in (
    ('20260909-6669-rank46-current-prefix','summary-6671.json'),
    ('20260909-6673-rank46-long-prefix','summary-6675.json'),
    ('20260909-6676-rank46-search-window','summary-6678.json'),
    ('20260909-6681-rank46-conflict-watch','summary-6683.json')):
    c=root/'benchmarks/runs'/name; m=json.loads((c/'manifest.json').read_text())
    for rel,digest in m['hashes'].items(): assert sha(c/rel)==digest,rel
    bindings(m['dependencies'])
    for filename in ('manifest.json','launch.json','run.py','array.sbatch',summary): own(c/filename)
    s=json.loads((c/summary).read_text()); bindings(s['hashes'])
    evidence[str((c/summary).relative_to(root))]=sha(c/summary)
    results=list((c/'results').glob('*.json')); assert len(results)==2
    for p in results:
        own(p); r=json.loads(p.read_text())
        assert r['exit_code']==0 and r['scratch_deleted']
        assert r['binary_sha256']==m['hashes']['pins/'+r['arm']]
        for rel,digest in r['trace_sha256'].items(): assert sha(c/'traces'/rel)==digest
    account(c,json.loads((c/'launch.json').read_text()),2,True)
for name in ('20260909-6579-material-schedule-build','20260909-6654-congruence-empty-gates-build'):
    c=root/'benchmarks/runs'/name; m=json.loads((c/'manifest.json').read_text())
    for rel,digest in m['files'].items(): assert sha(root/m['source']/rel)==digest,rel
    bindings(m['binaries'])
    for rel,digest in m.get('patches',{}).items(): assert sha(c/rel)==digest
    builds[str((c/'manifest.json').relative_to(root))]=sha(c/'manifest.json')
states={}
for name,count in (
    ('20260909-6661-congruence-empty-BC-gate',66),
    ('20260909-6663-rank49-congruence-empty-runtime',6),
    ('20260909-6666-congruence-empty-ranks1-50',50)):
    c=root/'benchmarks/runs'/name; m=json.loads((c/'manifest.json').read_text())
    for rel,digest in m.get('pin_sha256',{}).items(): assert sha(c/rel)==digest,rel
    for filename in ('manifest.json','launch.json','array.sbatch','matrix.tsv'):
        if (c/filename).exists(): own(c/filename)
    if count==6:
        submissions=sorted(c.glob('wave*-submission.json')); assert len(submissions)==3
        for p in submissions:
            own(p); launch=json.loads(p.read_text()); bindings(launch['preceding_records']); account(c,launch,2,True)
        p=c/'solver-checkpoint-6672.json'; own(p)
        s=json.loads(p.read_text()); bindings(s['hashes']); evidence[str(p.relative_to(root))]=sha(p)
    else: account(c,json.loads((c/'launch.json').read_text()),count)
    current={}
    for folder in ('provisional','results'):
        for p in sorted((c/folder).glob('*.json')):
            own(p); r=json.loads(p.read_text()); status=classify(r,folder=='results')
            assert status['status'] in ('pending_validation','checked_SAT','checked_UNSAT'),(p,status)
            assert r['task']==int(p.stem)
            if folder=='results': assert r['binary_sha256']==m['arms'][r['arm']]['sha256']
            current[r['task']]=dict(rank=r['rank'],arm=r['arm'],status=status,seconds=r['solve']['wall_seconds'])
    assert len(current)==count
    if count==6: assert all(r['status']['checked'] and r['status']['clean'] for r in current.values())
    states[name]=current
p=root/'benchmarks/reports/2026-09-09-congruence-empty-full50-6668/summary.json'
s=json.loads(p.read_text()); bindings(s['hashes']); evidence[str(p.relative_to(root))]=sha(p)
for filename in ('summary.json','comparison.tsv'): own(p.parent/filename)
checks={}
commands=[(n,['python3','-m','unittest','discover','-s','tools/'+n,'-p','test_*.py']) for n in ('bench','slurm')]
commands += [(n,['python3','tools/docs/'+n+'.py']) for n in ('check-doc-size','check-progress','check-rule-policy-boundary','check-proof-rule-sync','check-section-refs','check-citations')]
for name,command in commands:
    r=subprocess.run(command,cwd=root,capture_output=True,text=True,check=True)
    checks[name]=dict(command=command,stdout=r.stdout,stderr=r.stderr); print(name,'PASS',flush=True)
report=root/'report-to-andrew/20260909-1940-congruence-runtime-rank46'
assert (report/'report.pdf').read_bytes().startswith(b'%PDF-')
assert 'Overfull' not in (report/'report.log').read_text()
out=dict(observed_at=dt.datetime.now(dt.timezone.utc).isoformat(),
    prior_audit=dict(path=str(prior_path.relative_to(root)),sha256=sha(prior_path)),
    owned_hashes={rel:sha(root/rel) for rel in sorted(owned)},protected_campaign_hashes=prior['protected_campaign_hashes'],
    evidence=evidence,build_evidence=builds,accounting=accounting,campaign_states=states,checks=checks,
    andrew_report={str((report/n).relative_to(root)):sha(report/n) for n in ('report.tex','report.pdf')},
    cleanup='No manual deletion. Terminal diagnostic runners removed only their own hashed partial certificates and scratch. All live checker scratch, fuzz artifacts, pins, histories and ongoing campaigns left untouched.',
    limitations='Candidate6654 remains experimental/unpromoted/unfuzzed; no solver source edit this follow-up. Rank49 timing closes, memory does not; full50 still1.138354554 times fixedKissat with pending checks. Rank46 selected conflict follows reversed watch order; first movement/reconstruction cause remains open. No FlatLists change, fixed-reference refresh, fuzz transfer/artifact inspection or denied push retry. Initial report compile exposed path-line overfull boxes, corrected before publication.')
with target.open('x') as f: json.dump(out,f,indent=2); f.write('\n')
print(target)
for name,rows in states.items(): print(name,len(rows),'published',sum(r['status']['checked'] and r['status']['clean'] for r in rows.values()),'checked/clean')

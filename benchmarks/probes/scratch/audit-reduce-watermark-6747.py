#!/usr/bin/env python3
"""Freeze the isolated historical-watermark repair and published gate state."""
import datetime as dt
import json
from pathlib import Path
import re
import subprocess
import sys
root=Path(__file__).resolve().parents[3];sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
target=root/'benchmarks/reports/2026-09-09-reduce-watermark-audit-6747.json'
assert not target.exists()
prior_path=root/'benchmarks/reports/2026-09-09-rank46-implication-history-audit-6724.json'
prior=json.loads(prior_path.read_text())
for group in ('owned_hashes','protected_campaign_hashes','andrew_report'):
    for rel,digest in prior[group].items():
        if rel!='PROGRESS.md':assert sha(root/rel)==digest,rel
assert not subprocess.check_output(['git','diff','--','src','include','CMakeLists.txt'],cwd=root)
owned={'PROGRESS.md','docs/implementation-notes.md','benchmarks/reports/2026-09-09-rank46-reduce-watermark.md',
       'tools/slurm/session_deadline.py','tools/slurm/test_session_deadline.py',
       'benchmarks/probes/scratch/session-cutoff-20260910-0600/deadline.json'}
for p in (root/'benchmarks/probes/scratch').iterdir():
    match=re.search(r'-(\d+)\.(py|c)$',p.name)
    if match and 6726<=int(match[1])<=6748:owned.add(str(p.relative_to(root)))
def own(p):owned.add(str(p.relative_to(root)))
def bindings(hashes):
    for rel,digest in hashes.items():assert sha(root/rel)==digest,rel
evidence={};builds={};accounting={};states={}
def account(c,launch,count):
    assert launch['manifest_sha256']==sha(c/'manifest.json')
    command=['sacct','-n','-X','--array','-j',launch['job_id'],'-S',launch['accounting_since'],'-o','JobID,State%40,ExitCode,Submit','-P']
    stdout=subprocess.check_output(command,text=True)
    rows=stdout.splitlines();assert len(rows)==count,(c,stdout)
    for row in rows:
        cells=row.split('|')
        assert cells[1] in ('PENDING','RUNNING','COMPLETING','COMPLETED') and cells[2]=='0:0',row
        assert cells[3]>=launch['accounting_since']
    accounting[launch['job_id']]=dict(campaign=str(c.relative_to(root)),command=command,stdout=stdout)
for name,summary,count in (
 ('20260909-6721-rank46-earlier-allscan','summary-6723.json',2),
 ('20260909-6726-rank46-collect-boundary','summary-6729.json',2),
 ('20260909-6735-reduce-watermark-prefix','summary-6739.json',1)):
    c=root/'benchmarks/runs'/name;m=json.loads((c/'manifest.json').read_text())
    for rel,digest in m['hashes'].items():assert sha(c/rel)==digest,rel
    bindings(m['dependencies'])
    for filename in ('manifest.json','launch.json','run.py','array.sbatch',summary):own(c/filename)
    s=json.loads((c/summary).read_text());bindings(s['hashes']);evidence[str((c/summary).relative_to(root))]=sha(c/summary)
    records=list((c/'results').glob('*.json'));assert len(records)==count
    for p in records:
        own(p);r=json.loads(p.read_text());assert r['exit_code']==0 and r['scratch_deleted']
        assert r['binary_sha256']==m['hashes']['pins/'+r['arm']]
        for rel,digest in r['trace_sha256'].items():assert sha(c/'traces'/rel)==digest
    account(c,json.loads((c/'launch.json').read_text()),count)
assert not (root/'benchmarks/runs/20260909-6726-rank46-collect-boundary/summary-6728.json').exists()
for name in ('20260909-6654-congruence-empty-gates-build','20260909-6730-reduce-watermark-build'):
    c=root/'benchmarks/runs'/name;m=json.loads((c/'manifest.json').read_text())
    for rel,digest in m['files'].items():assert sha(root/m['source']/rel)==digest,rel
    bindings(m['binaries'])
    for rel,digest in m['patches'].items():assert sha(c/rel)==digest
    builds[str((c/'manifest.json').relative_to(root))]=sha(c/'manifest.json')
    if name.startswith('20260909-6730'):
        for filename in ('manifest.json','parent.json','incremental.patch','cumulative.patch'):own(c/filename)
        for rel,digest in m['logs'].items():assert sha(c/rel)==digest;own(c/rel)
for name in ('20260909-6733-reduce-watermark-native','20260909-6734-reduce-watermark-proof-control'):
    c=root/'benchmarks/runs'/name;s=json.loads((c/'summary.json').read_text());bindings(s['hashes'])
    own(c/'summary.json');evidence[str((c/'summary.json').relative_to(root))]=sha(c/'summary.json')
    for p in c.rglob('*'):
        if p.is_file() and p.suffix in ('.log','.cnf','.proof'):own(p)
c=root/'benchmarks/runs/20260909-6732-reduce-watermark-native'
assert not (c/'summary.json').exists();own(c/'compile.log')
for name,count in (
 ('20260909-6666-congruence-empty-ranks1-50',50),
 ('20260909-6737-reduce-watermark-BC-gate',66),
 ('20260909-6741-rank46-watermark-runtime',6),
 ('20260909-6743-reduce-watermark-ranks1-50',50)):
    c=root/'benchmarks/runs'/name;m=json.loads((c/'manifest.json').read_text())
    for rel,digest in m.get('pin_sha256',{}).items():assert sha(c/rel)==digest,rel
    for filename in ('manifest.json','launch.json','array.sbatch','matrix.tsv'):
        if (c/filename).exists():own(c/filename)
    if count==6:
        launches=sorted(c.glob('wave*-submission.json'));assert len(launches)==3
        for p in launches:
            own(p);launch=json.loads(p.read_text());bindings(launch['preceding_records']);account(c,launch,2)
    else:account(c,json.loads((c/'launch.json').read_text()),count)
    current={}
    for folder in ('provisional','results'):
        for p in sorted((c/folder).glob('*.json')):
            own(p);r=json.loads(p.read_text());status=classify(r,folder=='results')
            assert status['status'] in ('pending_validation','checked_SAT','checked_UNSAT'),(p,status)
            assert r['task']==int(p.stem)
            if folder=='results':assert r['binary_sha256']==m['arms'][r['arm']]['sha256']
            current[r['task']]=dict(rank=r['rank'],arm=r['arm'],status=status,seconds=r['solve']['wall_seconds'])
    assert len(current)<=count
    if count in (6,66) or name.startswith('20260909-6666'):assert len(current)==count
    states[name]=current
    for filename in ('summary-6740.json','solver-checkpoint-6745.json'):
        p=c/filename
        if p.exists():
            own(p);s=json.loads(p.read_text());bindings(s['hashes']);evidence[str(p.relative_to(root))]=sha(p)
checks={}
commands=[(n,['python3','-m','unittest','discover','-s','tools/'+n,'-p','test_*.py']) for n in ('bench','slurm')]
commands += [(n,['python3','tools/docs/'+n+'.py']) for n in ('check-doc-size','check-progress','check-rule-policy-boundary','check-proof-rule-sync','check-section-refs','check-citations')]
for name,command in commands:
    r=subprocess.run(command,cwd=root,capture_output=True,text=True,check=True)
    checks[name]=dict(command=command,stdout=r.stdout,stderr=r.stderr);print(name,'PASS',flush=True)
report=root/'report-to-andrew/20260909-2135-reduce-watermark'
assert (report/'report.pdf').read_bytes().startswith(b'%PDF-')
assert 'Overfull' not in (report/'report.log').read_text()
out=dict(observed_at=dt.datetime.now(dt.timezone.utc).isoformat(),
    prior_audit=dict(path=str(prior_path.relative_to(root)),sha256=sha(prior_path)),
    owned_hashes={rel:sha(root/rel) for rel in sorted(owned)},protected_campaign_hashes=prior['protected_campaign_hashes'],
    evidence=evidence,build_evidence=builds,accounting=accounting,campaign_states=states,checks=checks,
    andrew_report={str((report/n).relative_to(root)):sha(report/n) for n in ('report.tex','report.pdf')},
    cleanup='No manual deletion. Terminal diagnostic runners removed only their owned partial proofs and scratch. Published checker-clean records audited; all live checker scratch, fuzz artifacts, earlier pins and campaigns left untouched. Small fixed proof witnesses retained.',
    limitations='Isolated6730 experimental/unpromoted/unfuzzed. Rank46 full1M decisions match native after general historical-boundary repair; runtime repetitions and full50 sensitivity are separate. Prior6654 full50 timing remains immutable.6728 observer expectation and6732 fixture compile failures retained; neither was a solver invalidity. No FlatLists work, reference refresh, denied fuzz transfer/artifact access or push retry. Andrew report local and unsent.')
with target.open('x') as out_file:json.dump(out,out_file,indent=2);out_file.write('\n')
print(target)
for name,rows in states.items():print(name,len(rows),'published',sum(r['status']['checked'] and r['status']['clean'] for r in rows.values()),'checked/clean')

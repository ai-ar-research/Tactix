#!/usr/bin/env python3
"""Audit the semantic repair, all active dependencies and fixed fixture cleanup."""
import json
from pathlib import Path
import subprocess
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha,score
from checked_comparison import classify
target=root/'benchmarks/reports/2026-09-09-substitute-retry-audit-6250.json'
assert not target.exists()
owned=set(); local={}; runs={}; checks={}
def retain(p):
    rel=str(p.relative_to(root)); assert p.is_file()
    if p.stat().st_size>=1<<20 or rel.endswith('.decisions.tsv'):
        local[rel]=dict(bytes=p.stat().st_size,sha256=sha(p))
    else: owned.add(rel)
for rel in ('PROGRESS.md','docs/calculus.md','docs/implementation-notes.md',
            'benchmarks/reports/2026-09-09-substitute-retry-publication.md',
            'benchmarks/reports/2026-09-09-retry-fixture-cleanup-6245.json',
            'benchmarks/reports/2026-09-09-substitute-retry-checkpoint-6247.json'):
    retain(root/rel)
for n in range(6231,6252):
    for p in (root/'benchmarks/probes/scratch').glob('*'+str(n)+'.*'):
        if p.is_file() and p.suffix in ('.py','.c','.cpp'): retain(p)
build=root/'benchmarks/runs/20260909-6231-substitute-retry-build'
b=json.loads((build/'manifest.json').read_text())
for rel,digest in b['files'].items(): assert sha(root/b['source']/rel)==digest
for rel,digest in b['binaries'].items(): assert sha(root/rel)==digest
for rel,digest in b['patches'].items(): assert sha(build/rel)==digest
for p in build.iterdir():
    if p.is_file() and p.suffix in ('.json','.patch'): retain(p)
for p in (build/'logs').iterdir(): retain(p)
for kind,number in (('release',103),('bc',103),('trace',102)):
    log=(build/'logs'/f'{kind}-ctest.log').read_text()
    assert f'100% tests passed out of {number}' in log
    assert '***Failed' not in log and '***Exception' not in log
assert not subprocess.check_output(['git','diff','--','src','include','CMakeLists.txt'],cwd=root)
previous=json.loads((root/'benchmarks/reports/2026-09-09-checked-full50-audit-6229.json').read_text())
for rel,digest in previous['protected_campaign_hashes'].items(): assert sha(root/rel)==digest

for name in ('20260909-6236-substitute-retry-BC-gate','20260909-6237-substitute-retry-rank32-prefix',
             '20260909-6239-substitute-retry-ranks1-50','20260909-6248-repaired-rank32-membership'):
    c=root/'benchmarks/runs'/name; m=json.loads((c/'manifest.json').read_text())
    for rel,digest in m.get('pin_sha256',m.get('hashes')).items(): assert sha(c/rel)==digest
    for filename in ('manifest.json','launch.json','run.py','array.sbatch','matrix.tsv','summary.json'):
        if (c/filename).exists(): retain(c/filename)
    launch=json.loads((c/'launch.json').read_text())
    cmd=['sacct','-n','-X','--array','-j',launch['job_id'],'-S',launch['accounting_since'],'-o','JobID,State%40,ExitCode,Submit','-P']
    out=subprocess.check_output(cmd,text=True)
    states={}
    for folder in ('provisional','results'):
        if not (c/folder).exists(): continue
        for p in (c/folder).glob('*.json'):
            r=json.loads(p.read_text()); retain(p)
            if 'task' in r:
                state=classify(r,folder=='results'); assert state['status'] in ('pending_validation','checked_SAT','checked_UNSAT')
                states[r['task']]=state
            else: assert r['exit_code']==0 and r['scratch_deleted'] and not Path(r['scratch']).exists()
    runs[name]=dict(launch=launch,accounting=dict(command=cmd,stdout=out),classes=states)
    if 'ranks1-50' in name and len(states)==50: runs[name]['score']=score(c)
    if (c/'summary.json').exists():
        s=json.loads((c/'summary.json').read_text())
        for rel,digest in s.get('hashes',{}).items(): assert sha(root/rel)==digest; retain(root/rel)

for name in ('20260909-6234-substitute-retry-references','20260909-6241-substitute-retry-fixed-proofs',
             '20260909-6242-input-id-control','20260909-6243-substitute-retry-fixed-proofs','20260909-6244-substitute-retry-fixed-proofs'):
    c=root/'benchmarks/runs'/name
    assert not list(c.glob('*.cnf')) and not list(c.glob('*.proof'))
    for p in c.iterdir():
        if p.is_file() and p.suffix in ('.log','.json'): retain(p)
assert json.loads((root/'benchmarks/runs/20260909-6244-substitute-retry-fixed-proofs/summary.json').read_text())['checked']==12
commands=[(name,['python3','-m','unittest','discover','-s','tools/'+name,'-p','test_*.py']) for name in ('bench','slurm')]
commands += [(name,['python3','tools/docs/'+name+'.py']) for name in ('check-doc-size','check-progress','check-rule-policy-boundary','check-proof-rule-sync','check-section-refs','check-citations')]
for name,command in commands:
    p=subprocess.run(command,cwd=root,capture_output=True,text=True,check=True)
    checks[name]=dict(command=command,stdout=p.stdout,stderr=p.stderr); print(name,'PASS',flush=True)
report=root/'report-to-andrew/20260909-0257-substitute-retry'
log=(report/'report.log').read_text()
assert 'Overfull' not in log and 'Output written on report.pdf (2 pages,' in log
assert (report/'report.pdf').read_bytes().startswith(b'%PDF-')
target.write_text(json.dumps(dict(owned_hashes={rel:sha(root/rel) for rel in sorted(owned)},local_hash_bound=local,
    protected_campaign_hashes=previous['protected_campaign_hashes'],runs=runs,checks=checks,
    andrew_report={str((report/n).relative_to(root)):sha(report/n) for n in ('report.tex','report.pdf')},
    promotion='Not authorized: new full50, repetition, memory and Claude-owned source fuzz gates open. No push/transfer retry.',
    next='Validate6248 full membership against cached native6208 before further heap/policy repair; inspect full6239 and pending certificates; required repeated timing remains.'),indent=2)+'\n')
print(target)

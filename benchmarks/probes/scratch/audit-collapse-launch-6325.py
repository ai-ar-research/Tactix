#!/usr/bin/env python3
"""Audit the isolated launch checkpoint without touching running checker scratch."""
import json
from pathlib import Path
import subprocess
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
target=root/'benchmarks/reports/2026-09-09-collapse-launch-audit-6325.json'
assert not target.exists()
prior=json.loads((root/'benchmarks/reports/2026-09-09-rank32-later-audit-6317.json').read_text())
for rel,digest in prior['protected_campaign_hashes'].items():assert sha(root/rel)==digest,rel
assert not subprocess.check_output(['git','diff','--','src','include','CMakeLists.txt'],cwd=root)
a=root/'benchmarks/runs/20260909-6305-substitute-collapse-build/manifest.json'
b=json.loads(a.read_text())
for rel,digest in b['files'].items():assert sha(root/b['source']/rel)==digest,rel
for rel,digest in b['binaries'].items():assert sha(root/rel)==digest,rel
owned={'PROGRESS.md'};local={};runs={};checks={}
def retain(p):
    rel=str(p.relative_to(root))
    if p.stat().st_size>=1<<20 or p.suffix in ('.log','.out') or '/traces/' in rel:
        local[rel]=dict(bytes=p.stat().st_size,sha256=sha(p))
    else:owned.add(rel)
for n in range(6319,6327):
    for p in (root/'benchmarks/probes/scratch').glob('*'+str(n)+'.py'):retain(p)
for name in ('2026-09-09-collapse-checkpoint-6319.json','2026-09-09-collapse-full-checkpoint-6324.json'):retain(root/'benchmarks/reports'/name)
names=('20260909-6320-substitute-collapse-ranks1-50','20260909-6321-collapse-rank32-prefix',
    '20260909-6311-substitute-collapse-BC-gate','20260909-6275-vivify-binary-BC-gate',
    '20260909-6278-vivify-binary-ranks1-50','20260909-6239-substitute-retry-ranks1-50')
for name in names:
    c=root/'benchmarks/runs'/name;m=json.loads((c/'manifest.json').read_text());launch=json.loads((c/'launch.json').read_text())
    assert sha(c/'manifest.json')==launch['manifest_sha256']
    for rel,digest in m.get('pin_sha256',m.get('hashes',{})).items():assert sha(c/rel)==digest,rel
    command=['sacct','-n','-X','--array','-j',launch['job_id'],'-S',launch['accounting_since'],'-o','JobID,State%40,ExitCode,Submit','-P']
    accounting=subprocess.check_output(command,text=True);states={}
    for folder in ('provisional','results'):
        for p in sorted((c/folder).glob('*.json')):
            r=json.loads(p.read_text())
            if 'task' in r:
                state=classify(r,folder=='results')
                assert state['status'] in ('pending_validation','checked_SAT','checked_UNSAT'),(p,state)
                states[r['task']]=state
            else:
                assert r['exit_code']==0 and r['scratch_deleted'] and not Path(r['scratch']).exists()
                for filename,digest in r['trace_sha256'].items():
                    q=c/'traces'/filename;assert sha(q)==digest;retain(q)
            retain(p)
    if name.startswith(('20260909-6320','20260909-6321')):
        for p in c.rglob('*'):
            if p.is_file() and p.suffix in ('.py','.patch','.sbatch') or p.is_file() and p.name in ('manifest.json','launch.json','matrix.tsv','summary.json'):retain(p)
    runs[name]=dict(launch=launch,accounting=dict(command=command,stdout=accounting),states=states)
    print(name,len(states),'solver records',sum(s['checked'] for s in states.values()),'checked',flush=True)
commands=[(n,['python3','-m','unittest','discover','-s','tools/'+n,'-p','test_*.py']) for n in ('bench','slurm')]
commands += [(n,['python3','tools/docs/'+n+'.py']) for n in ('check-doc-size','check-progress','check-rule-policy-boundary','check-proof-rule-sync','check-section-refs','check-citations')]
for name,command in commands:
    p=subprocess.run(command,cwd=root,text=True,capture_output=True,check=True)
    checks[name]=dict(command=command,stdout=p.stdout,stderr=p.stderr);print(name,'PASS',flush=True)
report=root/'report-to-andrew/20260909-0439-collapse-full50-launch'
assert 'Overfull' not in (report/'report.log').read_text()
assert (report/'report.pdf').read_bytes().startswith(b'%PDF-')
target.write_text(json.dumps(dict(owned_hashes={rel:sha(root/rel) for rel in sorted(owned)},local_hash_bound=local,
    protected_campaign_hashes=prior['protected_campaign_hashes'],runs=runs,checks=checks,
    andrew_report={str((report/n).relative_to(root)):sha(report/n) for n in ('report.tex','report.pdf')},
    cleanup='No manual deletion; inherited and new live checker scratch untouched. Final result classification checks runner cleanup; bounded partial proof is not final certificate acceptance.',
    next='Validate6321 with6323 when terminal; score6320 only with all50 outcomes. Continue pending checks asynchronously. No promotion or tracker/FlatLists approval inferred; no fuzz inspection or denied push/transfer retry.'),indent=2)+'\n')
print(target)

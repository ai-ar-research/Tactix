#!/usr/bin/env python3
"""Preserve the successful audit and correct only its PDF failure-history prose."""
import json
from pathlib import Path
import subprocess
import sys

root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha

prior_path=root/'benchmarks/reports/2026-09-09-activation-memory-audit-6494.json'
target=root/'benchmarks/reports/2026-09-09-activation-memory-supplement-6497.json'
assert not target.exists()
a=json.loads(prior_path.read_text())
for group in ('owned_hashes','protected_campaign_hashes','andrew_report'):
    for rel,digest in a[group].items():
        if rel!='PROGRESS.md': assert sha(root/rel)==digest,rel
report=root/'report-to-andrew/20260909-1020-activation-memory'
assert sha(report/'report-overflow.log')=='6b74f03d32abcef2b2ec836aa7c8e2ae42fa8117c4cdf41fad11e6cf9a344cf3'
assert 'Overfull' not in (report/'report.log').read_text()
assert (report/'report.pdf').read_bytes().startswith(b'%PDF-')
assert not subprocess.check_output(['git','diff','--','src','include','CMakeLists.txt'],cwd=root)
owned=set(a['owned_hashes'])|{
    str(prior_path.relative_to(root)),
    'benchmarks/reports/2026-09-09-activation-memory-audit-erratum-6496.md',
    'benchmarks/probes/scratch/supplement-activation-memory-6497.py',
    'benchmarks/probes/scratch/land-activation-memory-6498.py',
}
commands=[(n,['python3','-m','unittest','discover','-s','tools/'+n,'-p','test_*.py']) for n in ('bench','slurm')]
commands += [(n,['python3','tools/docs/'+n+'.py']) for n in ('check-doc-size','check-progress','check-rule-policy-boundary','check-proof-rule-sync','check-section-refs','check-citations')]
for name,command in commands:
    p=subprocess.run(command,cwd=root,text=True,capture_output=True,check=True)
    a['checks'][name]=dict(command=command,stdout=p.stdout,stderr=p.stderr)
    print(name,'PASS',flush=True)
a['owned_hashes']={rel:sha(root/rel) for rel in sorted(owned)}
a['prior_audit']={'path':str(prior_path.relative_to(root)),'sha256':sha(prior_path)}
a['failure_classification']=a['failure_classification'].replace('No new PDF overflow.',
    'Preliminary PDF compile after shared-blocker addition had a2.69647pt overfull line; original log preserved, paragraph reflowed, twice-clean final PDF already hash-bound by6494.6496 corrects the stale no-overflow sentence without changing the frozen audit.')
a['local_hash_bound'][str((report/'report-overflow.log').relative_to(root))]=dict(bytes=(report/'report-overflow.log').stat().st_size,sha256=sha(report/'report-overflow.log'))
target.write_text(json.dumps(a,indent=2)+'\n')
print(target)

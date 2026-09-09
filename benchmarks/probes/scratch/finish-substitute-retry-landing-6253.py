#!/usr/bin/env python3
"""Some retained parent evidence is already tracked unchanged, not in diff."""
import json
from pathlib import Path
import subprocess
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
audit=root/'benchmarks/reports/2026-09-09-substitute-retry-audit-6250.json'
recovery=root/'benchmarks/reports/2026-09-09-substitute-retry-landing-6252.json'
a=json.loads(audit.read_text()); r=json.loads(recovery.read_text())
assert sha(audit)==r['audit_sha256']
for group in ('owned_hashes','protected_campaign_hashes','andrew_report'):
    for rel,digest in a[group].items(): assert sha(root/rel)==digest,rel
for rel,item in r['local_original_logs'].items():
    assert sha(root/rel)==item['sha256'] and (root/rel).read_text()==item['raw_text']
owned=(set(a['owned_hashes'])-set(r['local_original_logs']))|{str(audit.relative_to(root)),str(recovery.relative_to(root)),
    'benchmarks/probes/scratch/recover-substitute-retry-landing-6252.py',str(Path(__file__).resolve().relative_to(root))}
subprocess.run(['git','add','-f','--',str(Path(__file__).resolve().relative_to(root))],cwd=root,check=True)
staged=set(subprocess.check_output(['git','diff','--cached','--name-only'],cwd=root,text=True).splitlines())
assert staged <= owned, staged-owned
assert not subprocess.check_output(['git','diff','--name-only','--',*sorted(owned)],cwd=root)
subprocess.run(['git','diff','--cached','--check'],cwd=root,check=True)
subprocess.run(['git','diff','--cached','--stat'],cwd=root,check=True)
subprocess.run(['git','commit','-m','Restore canonical Substitute binary-removal retry publication'],cwd=root,check=True)
assert not subprocess.check_output(['git','status','--porcelain','--',*sorted(owned)],cwd=root)
print('PASS: owned checkpoint clean; raw logs preserved; no push or promotion')

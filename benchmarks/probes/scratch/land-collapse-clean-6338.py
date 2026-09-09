#!/usr/bin/env python3
"""Resume only the explicitly audited index after the patch-encoding check."""
import json
from pathlib import Path
import subprocess
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
prior=root/'benchmarks/reports/2026-09-09-collapse-outcome-audit-6335.json'
old=json.loads(prior.read_text())
path=root/'benchmarks/reports/2026-09-09-collapse-clean-audit-6337.json'
a=json.loads(path.read_text())
for group in ('owned_hashes','protected_campaign_hashes','andrew_report'):
    for rel,digest in a[group].items():assert sha(root/rel)==digest,rel
owned=set(a['owned_hashes'])|{str(path.relative_to(root))}
staged=set(subprocess.check_output(['git','diff','--cached','--name-only'],cwd=root,text=True).splitlines())
assert staged<=set(old['owned_hashes'])|{str(prior.relative_to(root))},'Unrelated staged changes; stop'
excluded=sorted(staged-owned)
assert set(excluded)<=set(a['local_hash_bound'])
if excluded:subprocess.run(['git','restore','--staged','--',*excluded],cwd=root,check=True)
subprocess.run(['git','add','-f','--',*sorted(owned)],cwd=root,check=True)
assert set(subprocess.check_output(['git','diff','--cached','--name-only'],cwd=root,text=True).splitlines())<=owned
subprocess.run(['git','diff','--cached','--check'],cwd=root,check=True)
subprocess.run(['git','diff','--cached','--stat'],cwd=root,check=True)
subprocess.run(['git','commit','-m','Record current full50 repair outcome and close the later Rank32 BVE split'],cwd=root,check=True)
assert not subprocess.check_output(['git','status','--porcelain','--',*sorted(owned)],cwd=root)
print('Owned checkpoint clean; frozen patch bytes preserved; no push.')

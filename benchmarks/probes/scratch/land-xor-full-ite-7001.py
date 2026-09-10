#!/usr/bin/env python3
"""Commit the explicitly audited paths only; no push or production promotion."""
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
audit = root/'benchmarks/reports/2026-09-10-xor-full-ite-audit-7000.json'
a = json.loads(audit.read_text())
for group in ('owned_hashes','protected_campaign_hashes','andrew_report'):
    for rel,digest in a[group].items(): assert sha(root/rel)==digest,rel
owned = sorted(set(a['owned_hashes']) | {str(audit.relative_to(root))})
assert str(Path(__file__).relative_to(root)) in owned
staged = lambda:set(subprocess.check_output(['git','diff','--cached','--name-only'],cwd=root,text=True).splitlines())
assert staged() <= set(owned),'preserve unrelated staged work'
subprocess.run(['git','add','-f','--',*owned],cwd=root,check=True)
assert staged() and staged() <= set(owned)
subprocess.run(['git','diff','--cached','--check'],cwd=root,check=True)
p = subprocess.run(['git','commit','-m','Measure full50 XOR neutrality and test ITE pair-load optimization'],cwd=root,capture_output=True,text=True)
assert p.returncode==0,(p.returncode,p.stdout[-3000:],p.stderr[-3000:])
assert not subprocess.check_output(['git','status','--porcelain','--',*owned],cwd=root)
print(subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True).strip())
print('Owned checkpoint clean. Production unchanged; no push retry.')

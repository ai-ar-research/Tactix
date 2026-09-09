#!/usr/bin/env python3
"""Land audited isolated-source patches and evidence; no promotion or push."""
import json
from pathlib import Path
import subprocess
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
p=root/'benchmarks/reports/2026-09-09-substitute-retry-audit-6250.json'
a=json.loads(p.read_text())
assert not subprocess.check_output(['git','diff','--cached','--name-only'],cwd=root)
for group in ('owned_hashes','protected_campaign_hashes','andrew_report'):
    for rel,digest in a[group].items(): assert sha(root/rel)==digest,rel
owned=sorted(set(a['owned_hashes'])|{str(p.relative_to(root)),str(Path(__file__).resolve().relative_to(root))})
subprocess.run(['git','add','-f','--',*owned],cwd=root,check=True)
subprocess.run(['git','diff','--cached','--check'],cwd=root,check=True)
subprocess.run(['git','diff','--cached','--stat'],cwd=root,check=True)
subprocess.run(['git','commit','-m','Restore canonical Substitute binary-removal retry publication'],cwd=root,check=True)
assert not subprocess.check_output(['git','status','--porcelain','--',*owned],cwd=root)
print('PASS: local owned checkpoint clean; production unchanged; no push')

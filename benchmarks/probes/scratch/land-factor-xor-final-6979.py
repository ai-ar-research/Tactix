#!/usr/bin/env python3
"""Commit only the final audited set; preserve frozen raw artifacts, never push."""
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
audit = root/'benchmarks/reports/2026-09-10-factor-xor-final-audit-6978.json'
a = json.loads(audit.read_text())
for group in ('owned_hashes','protected_campaign_hashes','andrew_report'):
    for rel,digest in a[group].items(): assert sha(root/rel) == digest,rel
owned = sorted(set(a['owned_hashes']) | {str(audit.relative_to(root))})
assert str(Path(__file__).relative_to(root)) in owned
staged = lambda:set(subprocess.check_output(['git','diff','--cached','--name-only'],cwd=root,text=True).splitlines())
assert staged() <= set(owned),'preserve unrelated staged work'
subprocess.run(['git','add','-f','--',*owned],cwd=root,check=True)
assert staged() and staged() <= set(owned)
subprocess.run(['git','diff','--cached','--check'],cwd=root,check=True)
subprocess.run(['git','diff','--cached','--stat'],cwd=root,check=True)
subprocess.run(['git','commit','-m','Measure Factor counter cost and reuse indexed XOR parity membership'],cwd=root,check=True)
assert not subprocess.check_output(['git','status','--porcelain','--',*owned],cwd=root)
print('Owned checkpoint clean. Production unchanged; no push retry.')

#!/usr/bin/env python3
"""Preserve frozen checker bytes; encode whitespace-bearing logs for Git."""
import json
from pathlib import Path
import subprocess
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
audit=root/'benchmarks/reports/2026-09-09-substitute-retry-audit-6250.json'
a=json.loads(audit.read_text())
target=root/'benchmarks/reports/2026-09-09-substitute-retry-landing-6252.json'
assert not target.exists()
logs={}
for rel,digest in a['owned_hashes'].items():
    assert sha(root/rel)==digest
    if not rel.endswith('-check.log'): continue
    raw=(root/rel).read_text()
    if any(line!=line.rstrip() for line in raw.splitlines()):
        logs[rel]=dict(sha256=digest,raw_text=raw)
assert len(logs)==10
for rel in logs:
    assert not subprocess.check_output(['git','ls-tree','HEAD','--',rel],cwd=root)
subprocess.run(['git','restore','--staged','--',*sorted(logs)],cwd=root,check=True)
for rel,item in logs.items(): assert sha(root/rel)==item['sha256']
target.write_text(json.dumps(dict(audit_sha256=sha(audit),local_original_logs=logs,
    correction='6251 staged raw drat-trim logs containing trailing spaces. Git whitespace validation rejected landing before commit. Preserve original bytes locally/hash-bound; this JSON encodes them exactly. No solver/checker/source/certificate change.'),indent=2)+'\n')
extra=[str(target.relative_to(root)),str(Path(__file__).resolve().relative_to(root))]
subprocess.run(['git','add','-f','--',*extra],cwd=root,check=True)
owned=sorted((set(a['owned_hashes'])-set(logs))|{str(audit.relative_to(root))}|set(extra))
assert set(subprocess.check_output(['git','diff','--cached','--name-only'],cwd=root,text=True).splitlines())==set(owned)
subprocess.run(['git','diff','--cached','--check'],cwd=root,check=True)
subprocess.run(['git','diff','--cached','--stat'],cwd=root,check=True)
subprocess.run(['git','commit','-m','Restore canonical Substitute binary-removal retry publication'],cwd=root,check=True)
assert not subprocess.check_output(['git','status','--porcelain','--',*owned],cwd=root)
print('PASS: local owned checkpoint clean; raw logs preserved; no push or promotion')

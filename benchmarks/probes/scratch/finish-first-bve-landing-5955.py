#!/usr/bin/env python3
"""Validate frozen unified-diff context separately from source whitespace."""
import hashlib
import json
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[3]
landing=root/'benchmarks/reports/2026-09-08-first-bve-attempts-landing.json'
target=root/'benchmarks/reports/2026-09-08-first-bve-attempts-landing-recovery.json'
assert not target.exists()
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
data=json.loads(landing.read_text())
for rel,digest in data['hashes'].items():assert sha(root/rel)==digest,rel
excluded=[]
for number in (5945,5949):
    c=next((root/'benchmarks/runs').glob(f'20260908-{number}-*'))
    m=json.loads((c/'manifest.json').read_text())
    path=c/'native-observer.patch'
    assert sha(path)==m['hashes']['native-observer.patch']
    expected=''
    for name in ('eventtrace.c','eliminate.c'):
        r=subprocess.run(['diff','-u','--label','a/src/'+name,'--label','b/src/'+name,
                          str(root/m['native_parent']/'src'/name),str(root/m['native_source']/'src'/name)],capture_output=True,text=True)
        assert r.returncode==1
        expected+=r.stdout
    assert path.read_text()==expected
    assert all(line==' ' for line in expected.splitlines() if line.rstrip()!=line)
    excluded.append(str(path.relative_to(root)))
cmd=['git','diff','--cached','--check','--','.',*(':(exclude)'+p for p in excluded)]
subprocess.run(cmd,cwd=root,check=True)
target.write_text(json.dumps(dict(landing_sha256=sha(landing),source_whitespace_check=cmd,
    frozen_patch_checks={p:sha(root/p) for p in excluded},
    recovery='Initial git whitespace check flags single-space blank context lines in unified diff data. Both frozen patches exactly regenerate from their bound old/new sources. Source/documentation whitespace check excludes only those two immutable data files; no artifact or checking policy changed.'),indent=2)+'\n')
subprocess.run(['git','add','-f','--',str(Path(__file__).relative_to(root)),str(target.relative_to(root))],cwd=root,check=True)
subprocess.run(cmd,cwd=root,check=True)
print('PASS: exact frozen patch regeneration and all other staged whitespace checks')

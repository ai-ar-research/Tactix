#!/usr/bin/env python3
"""Capture fixed CTests and current-source identity without rebuilding pins."""
import hashlib
import json
from pathlib import Path
import subprocess

root=Path(__file__).resolve().parents[3]
c=root/'benchmarks/runs/20260907-5359-focused-choice'
destination=c/'validation.json'
assert not destination.exists()
results=[]
for label,build in [('production','build/release'),('candidate','build/pin-5356-empty-factor-release')]:
    binary=root/build/'tactix';before=hashlib.sha256(binary.read_bytes()).hexdigest()
    r=subprocess.run(['ctest','--test-dir',build,'--output-on-failure','-j','8'],cwd=root,capture_output=True,text=True)
    (c/(label+'-ctest.log')).write_text(r.stdout+r.stderr)
    assert r.returncode==0 and '100% tests passed' in r.stdout
    assert hashlib.sha256(binary.read_bytes()).hexdigest()==before
    results.append(dict(label=label,build=build,exit=r.returncode,binary_sha256=before,log=label+'-ctest.log'))
destination.write_text(json.dumps(dict(results=results),indent=2)+'\n')
print(json.dumps(results))

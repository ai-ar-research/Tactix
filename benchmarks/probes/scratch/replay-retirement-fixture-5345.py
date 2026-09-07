#!/usr/bin/env python3
"""Fixed negative/positive policy regression; compile only declared live objects."""
import hashlib
import json
from pathlib import Path
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[3]
C = ROOT/'benchmarks/runs/20260907-5345-retirement-fixture'
C.mkdir(exist_ok=True)
source = ROOT/'benchmarks/probes/scratch/sweep-retirement-rounds-5345.cpp'
records = []
for arm,build,expected in (
    ('before',ROOT/'build/release',1),
    ('after',ROOT/'build/pin-5344-round-only',0)):
    link = build/'CMakeFiles/tactix.dir/link.txt'
    objects = [build/p for p in shlex.split(link.read_text()) if p.endswith('.o') and 'tactix_lib.dir' in p]
    output = ROOT/'benchmarks/probes/scratch'/('sweep-retirement-rounds-5345-'+arm)
    command = ['c++','-std=c++17','-O2','-DNDEBUG','-I'+str(ROOT/'src'),'-I'+str(ROOT/'include'),str(source),*map(str,objects),'-o',str(output)]
    inputs = {str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [source,link,*objects]}
    manifest = C/(arm+'-manifest.json')
    if manifest.exists():
        assert json.loads(manifest.read_text())['input_sha256'] == inputs, 'Pinned inputs changed; do not overwrite a frozen replay.'
    subprocess.run(command,check=True)
    result = subprocess.run([output],text=True,capture_output=True)
    assert result.returncode == expected, (arm,result.stdout,result.stderr)
    entry = dict(arm=arm,compile_command=command,input_sha256=inputs,binary_sha256=hashlib.sha256(output.read_bytes()).hexdigest())
    if not manifest.exists():
        manifest.write_text(json.dumps(entry,indent=2)+'\n')
    records.append(dict(arm=arm,expected_exit=expected,exit_code=result.returncode,stdout=result.stdout,stderr=result.stderr))
summary = json.dumps(dict(
    scope='Fixed heuristic-fidelity fixture, not a SAT solving benchmark or soundness failure.',
    conclusion='A productive disjoint Sweep precedes SCC retirement 3,6 in round one and 2 in round two. Current policy globally sorts to 2,3,6, altering a zero-score heap. The one-call removal preserves the canonical per-round pose.',
    fixture_development='Initial reachability assertion mistakenly required a new binary instead of a known equivalence; initial comparison had an empty focused-mode heap. Final fixture explicitly requires known-equivalence reachability and populates the score heap before comparing.',
    records=records),indent=2)+'\n'
path = C/'summary.json'
if path.exists():
    assert path.read_text() == summary
else:
    path.write_text(summary)
print(summary)

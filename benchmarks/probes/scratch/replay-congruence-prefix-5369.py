#!/usr/bin/env python3
"""Actual full-rule native/old/new fixed fixture; exact feature ABI."""
import hashlib
import json
from pathlib import Path
import shlex
import subprocess

root=Path(__file__).resolve().parents[3]
records=[]
def digest(p): return hashlib.sha256(p.read_bytes()).hexdigest()
for arm,build,expected in [('before','build/pin-5361-queue-bumps',1),
                           ('after','build/pin-5369-congruence-prefix',0)]:
 b=root/build
 link=b/'CMakeFiles/tactix.dir/link.txt'
 flags=b/'CMakeFiles/tactix.dir/flags.make'
 objects=[b/p for p in shlex.split(link.read_text()) if p.endswith('.o') and 'tactix_lib.dir' in p]
 lines=flags.read_text().splitlines()
 opts=[x for key in ('CXX_DEFINES','CXX_INCLUDES') for x in shlex.split(next(l.split('=',1)[1] for l in lines if l.startswith(key+' =')))]
 source=root/'benchmarks/probes/scratch/congruence-prefix-5369.cpp'
 binary=root/('benchmarks/probes/scratch/congruence-prefix-5369-'+arm)
 command=['c++','-std=c++20','-O2','-DNDEBUG',*opts,str(source),*map(str,objects),'-o',str(binary)]
 subprocess.run(command,check=True)
 r=subprocess.run([binary],capture_output=True,text=True)
 assert r.returncode==expected,(arm,r.returncode,r.stdout,r.stderr)
 records.append(dict(arm=arm,expected_exit=expected,exit=r.returncode,stdout=r.stdout,command=command,
  input_sha256={str(p.relative_to(root)):digest(p) for p in [source,link,flags,*objects]},binary_sha256=digest(binary)))
source=root/'benchmarks/probes/scratch/congruence-prefix-5369.c'
binary=root/'benchmarks/probes/scratch/congruence-prefix-5369-native'
command=['gcc','-O2','-DNDEBUG','-ffunction-sections','-fdata-sections','-Ikissat/src',str(source),'kissat/build/libkissat.a','-Wl,--gc-sections','-lm','-o',str(binary)]
subprocess.run(command,cwd=root,check=True)
r=subprocess.run([binary],capture_output=True,text=True,check=True)
records.append(dict(arm='native',exit=0,stdout=r.stdout,command=command,
 input_sha256={str(p.relative_to(root)):digest(p) for p in [source,root/'kissat/src/congruence.c',root/'kissat/build/libkissat.a']},binary_sha256=digest(binary)))
out=root/'benchmarks/runs/20260907-5369-congruence-prefix/fixture.json'
assert not out.exists()
out.write_text(json.dumps(dict(scope='Fixed full-rule fidelity test. Old exit 1 is the expected missing cleanup, not a solver soundness failure.',
 harness_note='Initial native fixture compile had a typedef/function name collision; renamed the fixture helper. No solver ran in that failed compile.',records=records),indent=2)+'\n')
for r in records: print(r['arm'],r['exit'],r['stdout'].strip())

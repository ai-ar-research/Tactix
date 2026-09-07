#!/usr/bin/env python3
"""Actual native/full-rule and old/new public-rule fixed retry fixtures."""
import hashlib
import json
from pathlib import Path
import shlex
import subprocess

root = Path(__file__).resolve().parents[3]
records = []
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
for arm, build, expected in [('before', 'build/pin-5377-retry-producers', 1),
                             ('after', 'build/pin-5378-vivify-retry', 0)]:
    b = root / build
    link = b / 'CMakeFiles/tactix.dir/link.txt'
    flags = b / 'CMakeFiles/tactix.dir/flags.make'
    objects = [b / p for p in shlex.split(link.read_text()) if p.endswith('.o') and 'tactix_lib.dir' in p]
    lines = flags.read_text().splitlines()
    opts = [x for key in ('CXX_DEFINES', 'CXX_INCLUDES') for x in shlex.split(next(l.split('=', 1)[1] for l in lines if l.startswith(key + ' =')))]
    source = root / 'benchmarks/probes/scratch/vivify-retry-5378.cpp'
    binary = root / ('benchmarks/probes/scratch/vivify-retry-5378-' + arm)
    command = ['c++', '-std=c++20', '-O2', '-DNDEBUG', *opts, str(source), *map(str, objects), '-o', str(binary)]
    subprocess.run(command, check=True)
    r = subprocess.run([binary], capture_output=True, text=True)
    assert r.returncode == expected, (arm, r.returncode, r.stdout, r.stderr)
    records.append(dict(arm=arm, expected_exit=expected, exit=r.returncode, stdout=r.stdout,
                        command=command, inputs={str(p.relative_to(root)): sha(p) for p in [source, link, flags, *objects]},
                        binary_sha256=sha(binary)))
source = root / 'benchmarks/probes/scratch/vivify-retry-5378.c'
binary = root / 'benchmarks/probes/scratch/vivify-retry-5378-native'
command = ['gcc', '-O2', '-DNDEBUG', '-ffunction-sections', '-fdata-sections', '-Ikissat/src', str(source), 'kissat/build/libkissat.a', '-Wl,--gc-sections', '-lm', '-o', str(binary)]
subprocess.run(command, cwd=root, check=True)
r = subprocess.run([binary], capture_output=True, text=True, check=True)
records.append(dict(arm='native', expected_exit=0, exit=0, stdout=r.stdout, command=command,
                    inputs={str(p.relative_to(root)): sha(p) for p in [source, root / 'kissat/src/vivify.c', root / 'kissat/build/libkissat.a']},
                    binary_sha256=sha(binary)))
p = root / 'benchmarks/runs/20260907-5378-vivify-retry/fixture.json'
result = dict(scope='Fixed mechanism regression. Old exit 1 is expected advice-fidelity failure, not solver unsoundness.', records=records)
if p.exists(): assert json.loads(p.read_text()) == result
else: p.write_text(json.dumps(result, indent=2) + '\n')
for r in records: print(r['arm'], r['exit'], r['stdout'].strip().splitlines()[-1])

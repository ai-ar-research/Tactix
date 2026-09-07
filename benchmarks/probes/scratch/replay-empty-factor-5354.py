#!/usr/bin/env python3
"""Native positive and old/new Tactix fixed fidelity test, not real solving."""
import hashlib
import json
from pathlib import Path
import shlex
import subprocess

root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260907-5354-empty-factor'
records = []
for arm, build, expected in (('before', 'build/pin-5353-factor-stamps', 1),
                             ('after', 'build/pin-5354-empty-factor', 0)):
    b = root / build
    link = b / 'CMakeFiles/tactix.dir/link.txt'
    objects = [b / p for p in shlex.split(link.read_text()) if p.endswith('.o') and 'tactix_lib.dir' in p]
    flags = (b / 'CMakeFiles/tactix.dir/flags.make').read_text().splitlines()
    defines = shlex.split(next(l.split('=', 1)[1] for l in flags if l.startswith('CXX_DEFINES =')))
    includes = shlex.split(next(l.split('=', 1)[1] for l in flags if l.startswith('CXX_INCLUDES =')))
    source = root / 'benchmarks/probes/scratch/factor-empty-stamps-5354.cpp'
    binary = root / ('benchmarks/probes/scratch/factor-empty-stamps-5354-' + arm)
    command = ['c++', '-std=c++20', '-O2', '-DNDEBUG', *defines, *includes,
               str(source), *map(str, objects), '-o', str(binary)]
    subprocess.run(command, check=True)
    result = subprocess.run([binary], capture_output=True, text=True)
    assert result.returncode == expected, (arm, result.returncode, result.stdout, result.stderr)
    records.append(dict(arm=arm, expected_exit=expected, exit=result.returncode, stdout=result.stdout,
        command=command, input_sha256={str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [source, link, *objects]},
        binary_sha256=hashlib.sha256(binary.read_bytes()).hexdigest()))
source = root / 'benchmarks/probes/scratch/factor-empty-stamps-5354.c'
binary = root / 'benchmarks/probes/scratch/factor-empty-stamps-5354-native'
command = ['gcc','-O2','-DNDEBUG','-ffunction-sections','-fdata-sections','-Ikissat/src',str(source),
           'kissat/build/libkissat.a','-Wl,--gc-sections','-lm','-o',str(binary)]
subprocess.run(command, cwd=root, check=True)
result = subprocess.run([binary], capture_output=True, text=True, check=True)
records.append(dict(arm='native', exit=0, stdout=result.stdout, command=command,
    input_sha256={str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [source, root/'kissat/src/factor.c',root/'kissat/build/libkissat.a']},
    binary_sha256=hashlib.sha256(binary.read_bytes()).hexdigest()))
destination = c / 'fixture.json'
assert not destination.exists()
destination.write_text(json.dumps(dict(scope='Fixed heuristic-fidelity replay; before exit 1 is the expected missing housekeeping, not solver unsoundness/crash.',
    harness_correction='The first replay compile omitted the trace-build feature definitions and failed its expected-result check. Fixed by taking exact CMake definitions/include paths from each linked build; no solver-correctness interpretation is made from the invalid-ABI executable.', records=records),indent=2)+'\n')
for r in records: print(r['arm'], r['exit'], r['stdout'].strip())

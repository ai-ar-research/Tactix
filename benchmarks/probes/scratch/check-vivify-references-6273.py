#!/usr/bin/env python3
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root/'tools/bench'))
from fixed_runtime import sha
c = root/'benchmarks/runs/20260909-6273-vivify-binary-references'
assert not c.exists(); c.mkdir(); records = {}
for arm, variant, extension, compiler, library in (
    ('native', 'kissat-schedule-continue-6206', 'c', 'gcc', 'libkissat.a'),
    ('cadical', 'cadical-extdiag', 'cpp', 'g++', 'libcadical.a')):
    source = root/'tools/solvers/variants'/variant
    fixture = root/f'benchmarks/probes/scratch/test-{arm}-vivify-binary-6273.{extension}'
    command = [compiler, '-O2', '-I'+str(source/'src'), '-I'+str(source/'build'), str(fixture), str(source/'build'/library), '-lm', '-o', str(c/arm)]
    with (c/(arm+'.log')).open('w') as log:
        subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=True)
        subprocess.run([str(c/arm)], stdout=log, stderr=subprocess.STDOUT, check=True)
    records[arm] = dict(command=command, fixture_sha256=sha(fixture), library_sha256=sha(source/'build'/library),
        source_sha256=sha(source/'src'/('vivify.c' if arm == 'native' else 'vivify.cpp')), output=(c/(arm+'.log')).read_text())
(c/'summary.json').write_text(json.dumps(records, indent=2)+'\n'); print({a:r['output'] for a,r in records.items()})

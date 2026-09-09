#!/usr/bin/env python3
"""Fixed actual reference calls, with no search invocation or source mutation."""
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
c = root / 'benchmarks/runs/20260909-6234-substitute-retry-references'
assert not c.exists(); c.mkdir()
result = {}
for arm, variant, suffix, compiler, library in (
    ('native', 'kissat-schedule-continue-6206', 'c', 'gcc', 'libkissat.a'),
    ('cadical', 'cadical-extdiag', 'cpp', 'g++', 'libcadical.a')):
    v = root / 'tools/solvers/variants' / variant
    fixture = root / f'benchmarks/probes/scratch/test-{arm}-substitute-retry-6234.{suffix}'
    command = [compiler, '-O2', '-I'+str(v/'src'), '-I'+str(v/'build'), str(fixture), str(v/'build'/library), '-lm', '-o', str(c/arm)]
    p = subprocess.run(command, capture_output=True, text=True)
    (c/(arm+'-build.log')).write_text(p.stdout+p.stderr); assert p.returncode == 0
    p = subprocess.run([str(c/arm)], cwd=c, capture_output=True, text=True)
    (c/(arm+'-run.log')).write_text(p.stdout+p.stderr)
    assert p.returncode == 0 and 'PASS: actual' in p.stdout
    result[arm] = dict(command=command, stdout=p.stdout, fixture_sha256=sha(fixture),
        library_sha256=sha(v/'build'/library), binary_sha256=sha(c/arm))
(c/'summary.json').write_text(json.dumps(result, indent=2)+'\n')
print(json.dumps(result, indent=2))

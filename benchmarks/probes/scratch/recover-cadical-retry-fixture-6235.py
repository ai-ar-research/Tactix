#!/usr/bin/env python3
"""Declare variables as the pinned CaDiCaL API requires; retain failed fixture."""
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root/'tools/bench'))
from fixed_runtime import sha
c = root/'benchmarks/runs/20260909-6234-substitute-retry-references'
v = root/'tools/solvers/variants/cadical-extdiag'
fixture = root/'benchmarks/probes/scratch/test-cadical-substitute-retry-6235.cpp'
out = c/'cadical-declared'; assert not out.exists()
command = ['g++','-O2','-I'+str(v/'src'),'-I'+str(v/'build'),str(fixture),str(v/'build/libcadical.a'),'-lm','-o',str(out)]
p = subprocess.run(command,capture_output=True,text=True)
(c/'cadical-declared-build.log').write_text(p.stdout+p.stderr); assert p.returncode == 0
p = subprocess.run([str(out)],cwd=c,capture_output=True,text=True)
(c/'cadical-declared-run.log').write_text(p.stdout+p.stderr); assert p.returncode == 0
assert 'PASS: actual CaDiCaL' in p.stdout
assert 'PASS: actual Kissat' in (c/'native-run.log').read_text()
(c/'summary.json').write_text(json.dumps(dict(command=command, stdout=p.stdout,
    native_stdout=(c/'native-run.log').read_text(),
    hashes={str(p.relative_to(root)):sha(p) for p in [fixture,v/'build/libcadical.a',out,c/'cadical-run.log',c/'native-run.log',c/'native',root/'tools/solvers/variants/kissat-schedule-continue-6206/build/libkissat.a']},
    correction='6234 omitted declare_more_variables, violating the pinned CaDiCaL factorcheck API contract before decomposition. 6235 fixes only fixture setup; original fatal-error output and source retained. No solver defect or repair.'),indent=2)+'\n')
print(p.stdout)

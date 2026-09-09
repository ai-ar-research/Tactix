#!/usr/bin/env python3
"""Run fixed new regression against actual frozen parent objects, no solver job."""
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
source = root / '.claude/worktrees/runtime-substitute-retry-6231'
c = root / 'benchmarks/runs/20260909-6231-substitute-retry-build'
parent = root / 'build/pin-6169-learned-sort-release'
out = c / 'parent-retry-fixture'
assert not out.exists()
objects = sorted((parent / 'CMakeFiles/tactix_lib.dir').rglob('*.o'))
assert objects
command = ['/cm/local/apps/gcc/13.1.0/bin/c++', '-std=c++20', '-O2', '-I' + str(source / 'src'),
           '-I' + str(source / 'include'), str(source / 'tests/test_substitute_retry.cpp'), *map(str, objects), '-o', str(out)]
p = subprocess.run(command, capture_output=True, text=True)
(c / 'logs/parent-fixture-build-v2.log').write_text(p.stdout + p.stderr)
assert p.returncode == 0
p = subprocess.run([str(out), '--expect-old-missing'], cwd=c, capture_output=True, text=True)
(c / 'logs/parent-fixture-run.log').write_text(p.stdout + p.stderr)
assert p.returncode == 0 and 'PASS: actual parent' in p.stdout
(c / 'parent-fixture.json').write_text(json.dumps(dict(command=command, stdout=p.stdout,
    fixture_sha256=sha(source / 'tests/test_substitute_retry.cpp'),
    objects={str(p.relative_to(root)): sha(p) for p in objects}), indent=2)+'\n')
print(p.stdout)

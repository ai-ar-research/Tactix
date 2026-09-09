#!/usr/bin/env python3
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
c = root / 'benchmarks/runs/20260909-6267-collapse-retry-fixture'
assert not c.exists(); c.mkdir()
fixture = root / 'benchmarks/probes/scratch/test-substitute-collapse-retry-6267.cpp'
t = root / '.claude/worktrees/runtime-substitute-retry-6231'
n = root / 'tools/solvers/variants/kissat-schedule-continue-6206'
objects = sorted((root / 'build/pin-6231-substitute-retry-release/CMakeFiles/tactix_lib.dir').rglob('*.o'))
assert objects
commands = {
    'candidate': ['g++', '-std=c++17', '-O2', '-I'+str(t/'src'), '-I'+str(t/'include'), str(fixture), *map(str, objects), '-o', str(c/'candidate')],
    'native': ['g++', '-std=c++17', '-O2', '-DNATIVE', '-I'+str(n/'src'), '-I'+str(n/'build'), str(fixture), str(n/'build/libkissat.a'), '-lm', '-o', str(c/'native')],
}
for arm, command in commands.items():
    with (c / (arm + '.log')).open('w') as log:
        subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=True)
        subprocess.run([str(c / arm)], cwd=c, stdout=log, stderr=subprocess.STDOUT, check=True)
summary = dict(commands=commands, fixture_sha256=sha(fixture),
    source_objects={str(p.relative_to(root)): sha(p) for p in objects},
    native_library_sha256=sha(n/'build/libkissat.a'),
    results={arm: (c/(arm+'.log')).read_text() for arm in commands},
    limitation='Actual implementations on fixed large-to-binary collapse: native re-arms retained endpoint, candidate does not. Not yet evidence that the selected Rank32 clause follows this path. No real benchmark solve or timing.')
(c / 'summary.json').write_text(json.dumps(summary, indent=2)+'\n')
print(summary['results'])

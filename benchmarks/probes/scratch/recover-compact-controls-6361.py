#!/usr/bin/env python3
"""Preserve failed controls; correct only fixture expectations/compiler binding."""
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root/'tools/bench'))
from fixed_runtime import sha
c = root/'benchmarks/runs/20260909-6361-compact-stamp-controls'
assert not c.exists()
c.mkdir()
records = {}
fixture = root/'.claude/worktrees/runtime-compact-stamp-6357/tests/test_compact_stamp.cpp'
code = fixture.read_text()
code = code.replace('static unsigned missing = 0, observations = 0;',
    'static unsigned missing = 0, observations = 0, disconnected_failures = 0;')
for name in ('mapped_first', 'mapped_other'):
    old = f'assert(!heuristics_queue_contains(k, {name}));'
    assert code.count(old) == 1
    code = code.replace(old, f'disconnected_failures += heuristics_queue_contains(k, {name});')
marker = '  // The frozen parent must miss every stamp observation, without crashing.'
assert code.count(marker) == 1
code = code.replace(marker,
    '  std::printf("disconnected_failures=%u\\n", disconnected_failures);\n'+marker)
(c/'parent-control.cpp').write_text(code)
source = root/'.claude/worktrees/runtime-substitute-collapse-6305'
objects = sorted((root/'build/pin-6305-substitute-collapse-release/CMakeFiles/tactix_lib.dir').rglob('*.o'))
command = ['/cm/local/apps/gcc/13.1.0/bin/c++', '-std=c++20', '-O2',
    '-I'+str(source/'src'), '-I'+str(source/'include'), str(c/'parent-control.cpp'),
    *map(str, objects), '-o', str(c/'parent')]
with (c/'parent-build.log').open('x') as out:
    subprocess.run(command, stdout=out, stderr=subprocess.STDOUT, check=True)
p = subprocess.run([str(c/'parent'), '--expect-old-missing'], cwd=c, text=True, capture_output=True)
(c/'parent.log').write_text(p.stdout+p.stderr)
assert p.returncode == 0, (p.returncode, p.stdout, p.stderr)
records['parent'] = dict(command=command, original_fixture_sha256=sha(fixture),
    objects={str(p.relative_to(root)):sha(p) for p in objects}, output=p.stdout,
    correction='Record detached-membership failures instead of asserting their absence on the known-broken predecessor; all other fixture assertions unchanged.')
native_fixture = Path(__file__).with_name('test-native-compact-stamp-6360.c')
code = native_fixture.read_text()
old = '        kissat *s = kissat_init();'
assert code.count(old) == 1
code = code.replace(old, old+'\n        kissat *solver = s; /* IDX assertions bind this identifier. */')
(c/'native-control.c').write_text(code)
source = root/'tools/solvers/variants/kissat-macro-heap-6090'
library = source/'build/libkissat.a'
command = ['gcc', '-O2', '-I'+str(source/'src'), '-I'+str(source/'build'),
    str(c/'native-control.c'), str(library), '-lm', '-o', str(c/'native')]
with (c/'native-build.log').open('x') as out:
    subprocess.run(command, stdout=out, stderr=subprocess.STDOUT, check=True)
p = subprocess.run([str(c/'native')], cwd=c, text=True, capture_output=True)
(c/'native.log').write_text(p.stdout+p.stderr)
assert p.returncode == 0, (p.returncode, p.stdout, p.stderr)
records['native'] = dict(command=command, original_fixture_sha256=sha(native_fixture),
    library_sha256=sha(library), source_sha256=sha(source/'src/compact.c'), output=p.stdout,
    correction='6360 failed compilation only: native IDX assertion macro requires a local solver pointer. No solver or fixture-state change.')
(c/'summary.json').write_text(json.dumps(dict(records=records,
    artifacts={str(p.relative_to(c)):sha(p) for p in c.iterdir() if p.is_file()},
    predecessor_failure='6359 assertion exposed zero-initialized detached link appearing self-connected at mapped index zero. Candidate preservation also restores disconnection; no unrelated solver failure.'), indent=2)+'\n')
for name, record in records.items():
    print(name, record['output'])

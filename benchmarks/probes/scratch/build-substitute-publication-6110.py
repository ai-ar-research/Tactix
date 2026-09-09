#!/usr/bin/env python3
"""Old/new actual-policy regression and independent Release builds."""
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
from pathlib import Path
import resource
import subprocess
root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260908-6107-substitute-publication'
source = root / '.claude/worktrees/runtime-substitute-publication-6107'
parent = root / '.claude/worktrees/runtime-reduce-root-lock-6049'
oldbuild = root / 'build/pin-6049-reduce-root-lock-release'
old = c / 'old-fixture'
def run(name, cmd, accepted=(0,)):
    path = c/'logs'/f'{name}.log'
    assert not path.exists(), path
    with path.open('w') as stream:
        p = subprocess.run(cmd, stdout=stream, stderr=subprocess.STDOUT,
                           preexec_fn=lambda: resource.setrlimit(resource.RLIMIT_CORE, (0, 0)))
    assert p.returncode in accepted, (name, p.returncode, path.read_text()[-5000:])
    print(name, p.returncode, 'PASS', flush=True)
run('old-build', ['g++', '-std=c++17', '-O2', '-I'+str(parent/'src'), '-I'+str(parent/'include'),
                 str(source/'tests/test_substitute_publication.cpp'),
                 *map(str, sorted((oldbuild/'CMakeFiles/tactix_lib.dir').rglob('*.o'))), '-o', str(old)])
run('old-negative', [str(old)], (-6,))
assert 'actual.h->scores.stack == expected.h->scores.stack' in (c/'logs/old-negative.log').read_text()
def build(number, name):
    out = root/f'build/pin-{number}-substitute-publication-{name}'
    assert not out.exists()
    run(name+'-configure', ['cmake', '-S', str(source), '-B', str(out), '-DCMAKE_BUILD_TYPE=Release'])
    run(name+'-build', ['cmake', '--build', str(out), '-j6'])
    run(name+'-ctest', ['ctest', '--test-dir', str(out), '--output-on-failure', '-j1'])
    return out/'tactix'
with ThreadPoolExecutor(max_workers=2) as pool:
    futures = [pool.submit(build, 6107, 'release'), pool.submit(build, 6108, 'bc')]
    binaries = [f.result() for f in futures]
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
assert sha(binaries[0]) == sha(binaries[1])
files = ['src/policies/default_policy.cpp', 'tests/test_substitute_publication.cpp']
(c/'build-summary.json').write_text(json.dumps(dict(
    source=str(source.relative_to(root)), parent=str(parent.relative_to(root)),
    files={r: sha(source/r) for r in files},
    binaries={str(p.relative_to(root)): sha(p) for p in binaries},
    tests='Eight actual-policy cases: budgeted/unbudgeted, initial/search, one/two round caps; exact heap/score/value/active state. Immutable6049 fails the heap assertion.',
    limitation='Isolated and unfuzzed; no runtime gain or promotion claimed.'), indent=2)+'\n')
print('PASS: independent Release pins identical', flush=True)

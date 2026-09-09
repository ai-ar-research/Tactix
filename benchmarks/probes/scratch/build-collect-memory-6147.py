#!/usr/bin/env python3
"""Prove exact Release identity and exercise GC checkpoint ordering."""
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
from pathlib import Path
import resource
import subprocess

root = Path(__file__).resolve().parents[3]
source = root / '.claude/worktrees/runtime-collect-memory-6146'
c = root / 'benchmarks/runs/20260908-6146-collect-memory-build'
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
def run(name, cmd, accepted=(0,)):
    path = c / 'logs' / (name + '.log')
    with path.open('x') as stream:
        p = subprocess.run(cmd, cwd=c, stdout=stream, stderr=subprocess.STDOUT,
            preexec_fn=lambda: resource.setrlimit(resource.RLIMIT_CORE, (0, 0)))
    assert p.returncode in accepted, (name, p.returncode, path.read_text()[-5000:])
    print(name, p.returncode, 'PASS', flush=True)
old = root / 'build/pin-6127-publication-trace'
run('old-build', ['g++', '-std=c++20', '-O2', '-DTACTIX_ENABLE_EVENT_TRACE=1',
    '-I' + str(source / 'src'), '-I' + str(source / 'include'),
    str(source / 'tests/test_collect_memory.cpp'),
    *map(str, sorted((old / 'CMakeFiles/tactix_lib.dir').rglob('*.o'))),
    '-o', str(c / 'old-fixture')])
run('old-negative', [str(c / 'old-fixture')], (-6,))
assert 'row == capacities.size() * stages.size()' in (c / 'logs/old-negative.log').read_text()
def build(number, kind, options):
    out = root / f'build/pin-{number}-collect-memory-{kind}'
    assert not out.exists(), out
    run(kind + '-configure', ['cmake', '-S', str(source), '-B', str(out),
        '-DCMAKE_BUILD_TYPE=Release', *options])
    run(kind + '-build', ['cmake', '--build', str(out), '-j6'])
    run(kind + '-ctest', ['ctest', '--test-dir', str(out), '--output-on-failure', '-j1'])
    return str(out.relative_to(root)), sha(out / 'tactix')
with ThreadPoolExecutor(max_workers=2) as pool:
    futures = [pool.submit(build, 6146, 'release', []),
               pool.submit(build, 6147, 'trace', ['-DTACTIX_ENABLE_EVENT_TRACE=ON'])]
    binaries = dict(f.result() for f in futures)
assert binaries['build/pin-6146-collect-memory-release'] == sha(root / 'build/pin-6136-substitute-journal-release/tactix')
inventory = {str(p.relative_to(source)): sha(p) for directory in ('src', 'include', 'tests')
             for p in sorted((source / directory).rglob('*')) if p.is_file()}
inventory['CMakeLists.txt'] = sha(source / 'CMakeLists.txt')
(c / 'summary.json').write_text(json.dumps(dict(source=str(source.relative_to(root)),
    parent='.claude/worktrees/runtime-substitute-journal-6136', binaries=binaries,
    release_byte_identical_to_6107=True, files=inventory,
    logs={str(p.relative_to(c)): sha(p) for p in (c / 'logs').iterdir()},
    hypothesis='Locate BVE peak before collector, during fresh/old arena overlap, or dense rebuilding; do not infer ownership from slot-end retained capacity.',
    test_consideration='Old actual trace objects lack required24 checkpoint rows. New fixed sparse,dense,empty collection cases verify8 stages, exact scratch ownership before/after swap, final arena identity and dense refs. Complete Release/trace suites; byte-identical Release is required before inheriting6107 gates.',
    limitation='Diagnostic instrumentation only; no production behavior change or memory/runtime improvement claim. Collector-local radix scratch and within-candidate allocations need finer sampling if the boundary evidence does not discriminate.'), indent=2) + '\n')
print('PASS: exact Release binary identity and complete fixed tests', flush=True)

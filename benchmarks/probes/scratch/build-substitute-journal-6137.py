#!/usr/bin/env python3
"""Reproduce missing transactions, then prove unchanged Release binary."""
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
from pathlib import Path
import resource
import subprocess

root = Path(__file__).resolve().parents[3]
source = root / '.claude/worktrees/runtime-substitute-journal-6136'
parent = root / '.claude/worktrees/runtime-substitute-publication-6107'
c = root / 'benchmarks/runs/20260908-6136-substitute-journal'
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()

def run(name, cmd, accepted=(0,)):
    path = c / 'logs' / (name + '.log')
    with path.open('x') as stream:
        p = subprocess.run(cmd, stdout=stream, stderr=subprocess.STDOUT,
            preexec_fn=lambda: resource.setrlimit(resource.RLIMIT_CORE, (0, 0)))
    assert p.returncode in accepted, (name, p.returncode, path.read_text()[-4000:])
    print(name, p.returncode, 'PASS', flush=True)

old = root / 'build/pin-6128-publication-journal'
run('old-build', ['g++', '-std=c++20', '-O2', '-DTACTIX_ENABLE_RULE_JOURNAL=1',
    '-I' + str(parent / 'src'), '-I' + str(parent / 'include'),
    str(source / 'tests/test_substitute_publication.cpp'),
    *map(str, sorted((old / 'CMakeFiles/tactix_lib.dir').rglob('*.o'))),
    '-o', str(c / 'old-fixture')])
run('old-negative', [str(c / 'old-fixture')], (-6,))
assert 'transactions == 1' in (c / 'logs/old-negative.log').read_text()

def build(number, kind, options):
    out = root / f'build/pin-{number}-substitute-journal-{kind}'
    assert not out.exists(), out
    run(kind + '-configure', ['cmake', '-S', str(source), '-B', str(out),
        '-DCMAKE_BUILD_TYPE=Release', *options])
    run(kind + '-build', ['cmake', '--build', str(out), '-j6'])
    run(kind + '-ctest', ['ctest', '--test-dir', str(out), '--output-on-failure', '-j1'])
    return str(out.relative_to(root)), sha(out / 'tactix')

with ThreadPoolExecutor(max_workers=2) as pool:
    futures = [pool.submit(build, 6136, 'release', []),
               pool.submit(build, 6137, 'journal', ['-DTACTIX_ENABLE_RULE_JOURNAL=ON'])]
    binaries = dict(f.result() for f in futures)
release = 'build/pin-6136-substitute-journal-release'
expected = sha(root / 'build/pin-6107-substitute-publication-release/tactix')
assert binaries[release] == expected, 'Release byte identity failed: no inherited timing claim'
(c / 'build-summary.json').write_text(json.dumps(dict(source=str(source.relative_to(root)),
    parent=str(parent.relative_to(root)), binaries=binaries,
    release_byte_identical_to_6107=True,
    files={name: sha(source / name) for name in ('src/policies/default_policy.cpp', 'tests/test_substitute_publication.cpp')},
    logs={str(p.relative_to(c)): sha(p) for p in (c / 'logs').iterdir()},
    test_consideration='Actual old journal objects fail missing-transaction assertion. New complete suites cover8 policycases, unbudgeted transaction currency, reconstruction/retirement/final-reconnect root and unchanged schedule-only budgeted path.',
    limitation='Restores unbudgeted caller transaction only;existing budgeted schedule-only coverage remains explicit debt. No production behavior/runtime gain;6107source fuzz gate remainspending.'), indent=2) + '\n')
print('PASS: new Release byte-identical to6107', flush=True)

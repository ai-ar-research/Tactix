#!/usr/bin/env python3
"""Independent Release/trace builds and unsuppressed fixed sanitizer gates."""
from concurrent.futures import ThreadPoolExecutor
import json
import os
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha

source = root / '.claude/worktrees/runtime-root-fold-7330'
campaign = root / 'benchmarks/runs/20260911-7332-root-fold-build'
assert not campaign.exists(), 'never overwrite successful or failed evidence'
(campaign / 'logs').mkdir(parents=True)
files = {str(p.relative_to(source)): sha(p)
         for folder in ('src', 'include', 'tests', 'examples')
         for p in sorted((source / folder).rglob('*')) if p.is_file()}
files['CMakeLists.txt'] = sha(source / 'CMakeLists.txt')
commands = {}

def run(label, command, env=None):
    command = list(map(str, command))
    result = subprocess.run(command, cwd=campaign, env=env,
                            capture_output=True, text=True)
    with (campaign / 'logs' / (label + '.json')).open('x') as out:
        json.dump(dict(command=command, returncode=result.returncode,
                       stdout=result.stdout, stderr=result.stderr), out, indent=2)
    commands[label] = dict(command=command, exit_code=result.returncode)
    assert result.returncode == 0, (label, result.stdout[-4000:], result.stderr[-4000:])
    print(label, 'PASS', flush=True)
    return result.stdout

fixtures = ('root_fold', 'compact_stamp', 'kernel', 'reconstruct',
            'propagation_cursor', 'api_c', 'api_instances', 'bve')

def build(kind):
    pin = root / ('build/pin-7332-root-fold-' + kind)
    assert not pin.exists(), pin
    args = ['cmake', '-S', source, '-B', pin,
            '-DCMAKE_BUILD_TYPE=' + ('Debug' if kind == 'sanitizers' else 'Release'),
            '-DTACTIX_ENABLE_EVENT_TRACE=' + ('ON' if kind == 'trace' else 'OFF'),
            '-DTACTIX_ENABLE_RULE_JOURNAL=OFF', '-DTACTIX_ENABLE_PROOF_SHADOW=OFF',
            '-DTACTIX_ENABLE_CONFLICT_STOP=OFF',
            '-DCMAKE_C_COMPILER=/cm/local/apps/gcc/13.1.0/bin/gcc',
            '-DCMAKE_CXX_COMPILER=/cm/local/apps/gcc/13.1.0/bin/g++']
    if kind == 'sanitizers':
        flags = '-fsanitize=address,undefined -fno-omit-frame-pointer'
        args += ['-DCMAKE_C_FLAGS=' + flags, '-DCMAKE_CXX_FLAGS=' + flags,
                 '-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined']
    run(kind + '-configure', args)
    targets = []
    if kind == 'sanitizers':
        targets = ['--target'] + ['test_' + name for name in fixtures]
    run(kind + '-build', ['cmake', '--build', pin, '-j2', *targets])
    if kind == 'sanitizers':
        env = dict(os.environ, ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',
                   UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
        for repeat in range(2):
            for name in fixtures:
                run(f'{kind}-{repeat}-{name}', [pin / 'tests' / ('test_' + name)], env)
        return kind, {str((pin / 'tests' / ('test_' + name)).relative_to(root)):
                      sha(pin / 'tests' / ('test_' + name)) for name in fixtures}
    output = run(kind + '-ctest', ['ctest', '--test-dir', pin,
                                 '--output-on-failure', '-j1'])
    count = 120 if kind == 'trace' else 121
    assert f'100% tests passed, 0 tests failed out of {count}' in output
    assert 'Skipped' not in output
    return kind, {str((pin / 'tactix').relative_to(root)): sha(pin / 'tactix')}

with ThreadPoolExecutor(max_workers=3) as pool:
    binaries = dict(pool.map(build, ('bc', 'trace', 'sanitizers')))
release = root / 'build/pin-7330-root-fold-release/tactix'
assert next(iter(binaries['bc'].values())) == sha(release)
hashes = {str((source / rel).relative_to(root)): digest for rel, digest in files.items()}
for rel, digest in files.items():
    assert sha(source / rel) == digest, ('source changed during validation', rel)
for p in [Path(__file__), release, *(campaign / 'logs').iterdir()]:
    hashes[str(p.relative_to(root))] = sha(p)
with (campaign / 'manifest.json').open('x') as out:
    json.dump(dict(status='PASS', source=str(source.relative_to(root)), files=files,
                   binaries=binaries, hashes=hashes, commands=commands,
                   test_consideration='Independent full Release/trace suites and eight fixed root/compaction/reason/API/model fixtures twice under unsuppressed ASan/UBSan/LSan. No source fuzz or real-instance execution.',
                   limitation='Isolated prototype; original-CNF 7331 microchecks, checked B28+C5, repeated memory/runtime and current-source fuzz remain separate gates.'), out, indent=2)
print(campaign / 'manifest.json')

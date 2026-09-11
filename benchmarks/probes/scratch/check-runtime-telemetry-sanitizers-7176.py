#!/usr/bin/env python3
"""Sanitize actual ablated kernel/registry plus fixed binary-propagation tests."""
import json
import os
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
source = root / '.claude/worktrees/runtime-telemetry-ablation-7172'
build = root / 'build/pin-7176-runtime-telemetry-sanitizers'
c = root / 'benchmarks/runs/20260910-7176-runtime-telemetry-sanitizers'
assert not build.exists() and not c.exists()
c.mkdir()
hashes = {str(p.relative_to(root)): sha(p) for d in ('src', 'include', 'tests')
          for p in sorted((source / d).rglob('*')) if p.is_file()}
hashes[str(Path(__file__).relative_to(root))] = sha(Path(__file__))
commands = []
def run(label, command):
    env = dict(os.environ, ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',
               UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    p = subprocess.run(command, cwd=c, capture_output=True, text=True, env=env)
    log = c / (label + '.log')
    with log.open('x') as out: out.write(p.stdout + p.stderr)
    hashes[str(log.relative_to(root))] = sha(log)
    commands.append(dict(label=label, command=command, exit_code=p.returncode))
    assert p.returncode == 0, (label, p.returncode, p.stdout, p.stderr)
    print(label, 'PASS', flush=True)
    return p.stdout
run('configure', ['cmake', '-S', str(source), '-B', str(build), '-DCMAKE_BUILD_TYPE=Debug',
    '-DTACTIX_DISABLE_RUNTIME_TELEMETRY=ON',
    '-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer',
    '-DCMAKE_C_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer',
    '-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined'])
fixtures = ['test_runtime_telemetry', 'test_backbone']
run('build', ['cmake', '--build', str(build), '-j4', '--target', *fixtures])
outcomes = {}
for fixture in fixtures:
    binary = build / 'tests' / fixture
    hashes[str(binary.relative_to(root))] = sha(binary)
    outcomes[fixture] = [run(fixture + f'-{i}', [str(binary)]) for i in range(2)]
    assert outcomes[fixture][0] == outcomes[fixture][1], fixture
for rel, digest in hashes.items(): assert sha(root / rel) == digest, rel
with (c / 'summary.json').open('x') as out:
    json.dump(dict(status='PASS', hashes=hashes, commands=commands, outcomes=outcomes,
        scope='Actual DebugASan/UBSan/LSan ablated library, runtime telemetry and complete Backbone fixtures twice each outside ptrace. No fuzz or real-instance execution.'), out, indent=2)

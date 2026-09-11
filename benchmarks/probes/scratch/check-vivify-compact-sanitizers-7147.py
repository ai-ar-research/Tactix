#!/usr/bin/env python3
"""Instrument the actual compact Vivify implementation and fixed fixtures."""
import json
import os
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
source = root / '.claude/worktrees/runtime-vivify-compact-7144'
build = root / 'build/pin-7144-vivify-compact-sanitizers'
c = root / 'benchmarks/runs/20260910-7147-vivify-compact-sanitizers'
assert not build.exists() and not c.exists()
c.mkdir()
hashes = {str(p.relative_to(root)): sha(p) for d in ('src', 'include', 'tests')
          for p in sorted((source / d).rglob('*')) if p.is_file()}
hashes[str(Path(__file__).relative_to(root))] = sha(Path(__file__))
commands = []
def run(label, command):
    env = dict(os.environ, ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',
               UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    result = subprocess.run(command, cwd=c, capture_output=True, text=True, env=env)
    p = c / (label + '.log')
    with p.open('x') as out: out.write(result.stdout + result.stderr)
    hashes[str(p.relative_to(root))] = sha(p)
    commands.append(dict(label=label, command=command, exit_code=result.returncode))
    assert result.returncode == 0, (label, result.returncode, result.stdout, result.stderr)
    print(label, 'PASS', flush=True)
    return result.stdout

run('configure', ['cmake', '-S', str(source), '-B', str(build), '-DCMAKE_BUILD_TYPE=Debug',
    '-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer',
    '-DCMAKE_C_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer',
    '-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined'])
fixtures = ['test_vivify', 'test_vivify_max', 'test_vivify_n1',
            'test_vivify_count_cache', 'test_vivify_order_contract', 'test_vivify_compact_contract']
run('build', ['cmake', '--build', str(build), '-j4', '--target', *fixtures])
outcomes = {}
for fixture in fixtures:
    binary = build / 'tests' / fixture
    hashes[str(binary.relative_to(root))] = sha(binary)
    outcomes[fixture] = [run(fixture + f'-{i}', [str(binary)]) for i in range(2)]
    assert outcomes[fixture][0] == outcomes[fixture][1], fixture
assert outcomes['test_vivify_order_contract'][0] == 'vivify-order-contract cases=96 digest=c67723e59482dad7\n'
assert outcomes['test_vivify_compact_contract'][0] == 'vivify-compact-contract cases=1728 digest=11603ba2e1d600bb proof-bytes=39624 subsumed=1872\n'
for rel, digest in hashes.items(): assert sha(root / rel) == digest, rel
with (c / 'summary.json').open('x') as out:
    json.dump(dict(status='PASS', hashes=hashes, commands=commands, outcomes=outcomes,
        scope='Actual DebugASan/UBSan/LSan library and six fixed Vivify fixture binaries, twice each, outside ptrace. Not a fuzz or real-instance solver run.'), out, indent=2)

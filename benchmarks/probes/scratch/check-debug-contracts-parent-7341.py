#!/usr/bin/env python3
"""Preserve failing old collection and positive/negative binary diagnostic gates."""
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
campaign = root / 'benchmarks/runs/20260911-7341-debug-contracts-regression'
assert not campaign.exists()
campaign.mkdir()
source = root / '.claude/worktrees/runtime-debug-contracts-7339'
parent = root / '.claude/worktrees/runtime-root-watermark-7333'
test = source / 'tests/test_collect_connection.cpp'
objects = sorted((root / 'build/pin-7335-root-watermark-release/CMakeFiles/tactix_lib.dir').rglob('*.o'))
assert objects
binary = campaign / 'parent-test-collect-connection'
records, hashes = [], {}
def run(name, command, expected):
    command = list(map(str, command))
    result = subprocess.run(command, cwd=campaign, capture_output=True, text=True)
    record = dict(name=name, command=command, returncode=result.returncode,
                  stdout=result.stdout, stderr=result.stderr)
    path = campaign / (name + '.json')
    with path.open('x') as out: json.dump(record, out, indent=2)
    hashes[str(path.relative_to(root))] = sha(path)
    records.append(record)
    assert result.returncode == expected, record
    print(name, result.returncode, flush=True)
run('build', ['/cm/local/apps/gcc/13.1.0/bin/g++', '-std=c++20', '-O3', '-DNDEBUG',
              '-fno-exceptions', '-fno-rtti', '-I' + str(parent / 'src'),
              '-I' + str(parent / 'include'), test, *objects, '-o', binary], 0)
run('old-collector', [binary], -6)
assert '!disconnected' in records[-1]['stderr']
checker = source / 'tests/test_no_event_trace_symbols.py'
for name, exe, args, expected in (
    ('debug-stubs', root / 'build/pin-7337-root-parent-debug/tactix', ['--debug-stubs'], 0),
    ('debug-not-zero-overhead', root / 'build/pin-7337-root-parent-debug/tactix', [], 1),
    ('reject-real-trace', root / 'build/pin-7335-root-watermark-trace/tactix', ['--debug-stubs'], 1),
    ('release-zero-overhead', root / 'build/pin-7335-root-watermark-release/tactix', [], 0)):
    run(name, ['python3', checker, exe, *args], expected)
    hashes[str(exe.relative_to(root))] = sha(exe)
for path in [Path(__file__), test, checker, binary, *objects]:
    hashes[str(path.relative_to(root))] = sha(path)
with (campaign / 'manifest.json').open('x') as out:
    json.dump(dict(status='PASS_NEGATIVE_CONTROLS', hashes=hashes, records=records,
                   scope='Actual parent collector installs large watches while disconnected/no index. Debug no-op validation accepts only whitelisted weak stubs with no protocol strings; real trace binary rejected; optimized zero-symbol condition unchanged.'), out, indent=2)

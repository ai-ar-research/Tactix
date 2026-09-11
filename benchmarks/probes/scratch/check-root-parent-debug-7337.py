#!/usr/bin/env python3
"""Classify the old Debug-suite failures against the untouched 7302 parent."""
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
authority = root / 'benchmarks/runs/20260911-7302-clause-realloc-build/manifest.json'
parent = json.loads(authority.read_text())
source = root / parent['source']
for rel, digest in parent['files'].items(): assert sha(source / rel) == digest, rel
campaign = root / 'benchmarks/runs/20260911-7337-root-parent-debug'
pin = root / 'build/pin-7337-root-parent-debug'
assert not campaign.exists() and not pin.exists()
(campaign / 'logs').mkdir(parents=True)
hashes = {str(authority.relative_to(root)): sha(authority)}
records = []
def run(label, command):
    command = list(map(str, command))
    result = subprocess.run(command, cwd=campaign, capture_output=True, text=True)
    record = dict(label=label, command=command, returncode=result.returncode,
                  stdout=result.stdout, stderr=result.stderr)
    path = campaign / 'logs' / (label + '.json')
    with path.open('x') as out: json.dump(record, out, indent=2)
    hashes[str(path.relative_to(root))] = sha(path)
    records.append(record)
    print(label, result.returncode, flush=True)
    return result
args = ['cmake', '-S', source, '-B', pin, '-DCMAKE_BUILD_TYPE=Debug',
        '-DTACTIX_ENABLE_EVENT_TRACE=OFF', '-DTACTIX_ENABLE_RULE_JOURNAL=OFF',
        '-DTACTIX_ENABLE_PROOF_SHADOW=OFF', '-DTACTIX_ENABLE_CONFLICT_STOP=OFF',
        '-DCMAKE_C_COMPILER=/cm/local/apps/gcc/13.1.0/bin/gcc',
        '-DCMAKE_CXX_COMPILER=/cm/local/apps/gcc/13.1.0/bin/g++']
assert run('configure', args).returncode == 0
tests = ('forward_unit', 'substitute_collapse', 'substitute_retry', 'subsume_material')
assert run('build', ['cmake', '--build', pin, '-j4', '--target', 'tactix',
                     *['test_' + name for name in tests]]).returncode == 0
for name in tests:
    binary = pin / 'tests' / ('test_' + name)
    result = run(name, [binary])
    assert result.returncode == -6 and 'Assertion' in result.stderr, (name, result.stdout, result.stderr)
    hashes[str(binary.relative_to(root))] = sha(binary)
checker = source / 'tests/test_no_event_trace_symbols.py'
result = run('event_trace_zero_overhead', ['python3', checker, pin / 'tactix'])
assert result.returncode != 0, 'expected original Debug symbol-test failure did not reproduce'
for p in [Path(__file__), checker, pin / 'tactix']:
    hashes[str(p.relative_to(root))] = sha(p)
for rel, digest in parent['files'].items(): assert sha(source / rel) == digest, rel
with (campaign / 'manifest.json').open('x') as out:
    json.dump(dict(status='REPRODUCED_ON_UNCHANGED_PARENT', hashes=hashes,
                   records=records, source=str(source.relative_to(root)),
                   scope='Five fixed Debug failures also occur on unchanged7302. They remain failures, not waived correctness or a passing full Debug suite. The intentionally stopped old demo is separate.'), out, indent=2)

#!/usr/bin/env python3
"""Correct only the standalone fixture ABI; preserve failed6207 evidence."""
import difflib
import json
from pathlib import Path
import resource
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
c = root / 'benchmarks/runs/20260909-6206-schedule-continue-build'
assert not (c / 'summary.json').exists()
parents = json.loads((c / 'parents.json').read_text())
sources = list(parents['parents'])
assert 'WEXITSTATUS(status)' in (c / 'logs/native-test.log').read_text()
fixture = root / 'benchmarks/probes/scratch/test-native-schedule-continue-6214.c'
assert fixture.read_text().startswith('#define NDEBUG 1\n#include "internal.h"\n#include "eventtrace.h"\n#undef NDEBUG\n')
def run(name, cmd):
    with (c / 'logs' / (name + '.log')).open('x') as log:
        r = subprocess.run(cmd, stdout=log, stderr=subprocess.STDOUT)
    assert r.returncode == 0, (name, (c / 'logs' / (name + '.log')).read_text())
    print(name, 'PASS', flush=True)
new = root / sources[1]; old = root / parents['parents'][sources[1]]['source']
for name, src in (('native-corrected', new), ('native-old-control', old)):
    run(name + '-compile', ['gcc', '-O2', '-I' + str(src / 'src'), str(fixture),
        str(src / 'build/libkissat.a'), '-lm', '-o', str(c / name)])
run('native-corrected', [str(c / 'native-corrected')])
resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
with (c / 'logs/native-old-expected-failure.log').open('x') as log:
    p = subprocess.Popen([str(c / 'native-old-control')], stdout=log, stderr=subprocess.STDOUT)
    pid = p.pid; status = p.wait()
assert status == -6 and 'WEXITSTATUS(status)' in (c / 'logs/native-old-expected-failure.log').read_text()
# The deliberately failing old control exits before its final fixture cleanup.
leftover = Path('/tmp') / f'tactix-native-schedule-{pid}-1.tsv'
assert leftover.is_file() and leftover.read_text().startswith('# bve-schedule-observer\t1\n')
removed = dict(path=str(leftover), bytes=leftover.stat().st_size, sha256=sha(leftover))
leftover.unlink()
assert not leftover.exists()
print('native old-library expected continuation failure PASS; exact fixture residue removed', flush=True)
inputs = {}; changes = {}; binaries = {}
for rel, parent in parents['parents'].items():
    inputs[rel] = {}; delta = []
    for name, digest in parent['files'].items():
        assert sha(root / parent['source'] / name) == digest
        inputs[rel][name] = sha(root / rel / name)
        if inputs[rel][name] != digest:
            delta.append(name)
            patch = ''.join(difflib.unified_diff((root / parent['source'] / name).read_text().splitlines(True),
                (root / rel / name).read_text().splitlines(True), fromfile='a/' + name, tofile='b/' + name, n=3))
            target = c / (('tactix-' if rel == sources[0] else 'native-') + Path(name).name + '.patch')
            assert not target.exists(); target.write_text(patch)
    changes[rel] = delta
assert set(changes[sources[0]]) == {'src/diagnostics/event_trace.cpp', 'tests/test_event_trace.cpp'}
assert changes[sources[1]] == ['src/eventtrace.c']
for kind, count in (('release', 99), ('trace', 98)):
    assert f'100% tests passed out of {count}' in (c / 'logs' / (kind + '-ctest.log')).read_text()
    path = root / ('build/pin-6206-schedule-continue-' + kind) / 'tactix'
    binaries[str(path.relative_to(root))] = sha(path)
assert binaries['build/pin-6206-schedule-continue-release/tactix'] == '86a251c9fec53cdd54c338a5b6cbc32aafe7f117b4ee06ea8e10c22d50bee235'
binaries[str((new / 'build/kissat').relative_to(root))] = sha(new / 'build/kissat')
(c / 'summary.json').write_text(json.dumps(dict(sources=inputs, parents_sha256=sha(c / 'parents.json'), changes=changes,
    binaries=binaries, logs={str(p.relative_to(c)): sha(p) for p in (c / 'logs').iterdir()}, release_exact_parent=True,
    fixture_sha256=sha(fixture), removed_negative_control_fixture=removed,
    failed_check='Initial6206 native fixture undefines NDEBUG before internal.h, selecting debug-only struct fields while linked library uses-DNDEBUG. This is an ABI-invalid harness,not a solver soundness/crash reproducer. Corrected6214 selects the library layout before re-enabling assertions; no native solver source correction was needed.',
    tests='Release99/99 andtrace98/98 including stopped/continued member snapshots,conflict/ordinal filters and no-overwrite. Corrected native empty fixture passes;same fixture against unchangedold library intentionally fails continuation expectation. Full real-parent replay remains mandatory.',
    limitation='Diagnostic-only continuation. No Release byte change, policy or proof-rule edit, timing acceptance, current-source fuzz closure or promotion.'), indent=2) + '\n')
print(json.dumps(binaries, indent=2))

#!/usr/bin/env python3
"""Build and test both observer adapters plus a bit-identical ordinary Release."""
from concurrent.futures import ThreadPoolExecutor
import difflib
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
c = root / 'benchmarks/runs/20260909-6206-schedule-continue-build'
assert not (c / 'summary.json').exists()
parents = json.loads((c / 'parents.json').read_text())
sources = list(parents['parents'])
for rel, parent in parents['parents'].items():
    for name, digest in parent['files'].items(): assert sha(root / parent['source'] / name) == digest
inputs = {rel: {name: sha(root / rel / name) for name in parent['files']}
          for rel, parent in parents['parents'].items()}
def run(name, cmd, cwd=root):
    p = c / 'logs' / (name + '.log')
    with p.open('x') as log:
        result = subprocess.run(cmd, cwd=cwd, stdout=log, stderr=subprocess.STDOUT)
    assert result.returncode == 0, (name, p.read_text()[-6000:])
    print(name, 'PASS', flush=True)
def tactix(kind, enabled):
    build = root / ('build/pin-6206-schedule-continue-' + kind)
    assert not build.exists()
    run(kind + '-configure', ['cmake', '-S', str(root / sources[0]), '-B', str(build),
        '-DCMAKE_BUILD_TYPE=Release', '-DTACTIX_ENABLE_CONFLICT_STOP=OFF',
        '-DTACTIX_ENABLE_EVENT_TRACE=' + enabled])
    run(kind + '-build', ['cmake', '--build', str(build), '-j6'])
    run(kind + '-ctest', ['ctest', '--test-dir', str(build), '--output-on-failure', '-j1'])
    return str((build / 'tactix').relative_to(root)), sha(build / 'tactix')
def native():
    src = root / sources[1]
    assert not (src / 'build').exists()
    run('native-configure', ['./configure'], src)
    run('native-build', ['make', '-j6'], src)
    test = c / 'test-native'
    run('native-test-build', ['gcc', '-O2', '-I' + str(src / 'src'),
        str(root / 'benchmarks/probes/scratch/test-native-schedule-continue-6206.c'),
        str(src / 'build/libkissat.a'), '-lm', '-o', str(test)])
    run('native-test', [str(test)])
    # This deliberate negative control must fail its continuation expectation
    # against the unchanged parent's library, not by a compile/link error.
    old = root / parents['parents'][sources[1]]['source']
    control = c / 'test-native-old'
    run('native-old-test-build', ['gcc', '-O2', '-I' + str(old / 'src'),
        str(root / 'benchmarks/probes/scratch/test-native-schedule-continue-6206.c'),
        str(old / 'build/libkissat.a'), '-lm', '-o', str(control)])
    with (c / 'logs/native-old-negative.log').open('x') as log:
        r = subprocess.run([str(control)], stdout=log, stderr=subprocess.STDOUT)
    assert r.returncode == -6 and 'WEXITSTATUS(status)' in (c / 'logs/native-old-negative.log').read_text()
    print('native-old-negative expected failure PASS', flush=True)
    return str((src / 'build/kissat').relative_to(root)), sha(src / 'build/kissat')
with ThreadPoolExecutor(max_workers=3) as pool:
    futures = [pool.submit(tactix, 'release', 'OFF'), pool.submit(tactix, 'trace', 'ON'), pool.submit(native)]
    binaries = dict(f.result() for f in futures)
assert binaries['build/pin-6206-schedule-continue-release/tactix'] == '86a251c9fec53cdd54c338a5b6cbc32aafe7f117b4ee06ea8e10c22d50bee235'
changes = {}
for rel, files in inputs.items():
    delta = []
    parent = parents['parents'][rel]
    for name, digest in files.items():
        assert sha(root / rel / name) == digest
        if digest != parent['files'][name]:
            delta.append(name)
            patch = ''.join(difflib.unified_diff((root / parent['source'] / name).read_text().splitlines(True),
                (root / rel / name).read_text().splitlines(True), fromfile='a/' + name, tofile='b/' + name, n=3))
            (c / (('tactix-' if rel == sources[0] else 'native-') + Path(name).name + '.patch')).write_text(patch)
    changes[rel] = delta
assert changes[sources[0]] == ['src/diagnostics/event_trace.cpp', 'tests/test_event_trace.cpp']
assert changes[sources[1]] == ['src/eventtrace.c']
(c / 'summary.json').write_text(json.dumps(dict(sources=inputs, parents_sha256=sha(c / 'parents.json'),
    changes=changes, binaries=binaries, logs={str(p.relative_to(c)): sha(p) for p in (c / 'logs').iterdir()},
    release_exact_parent=True, test_consideration='Existing actual Tactix schedule fixture now covers both exit/return modes and later ordinal no-overwrite with unchanged counts. Native empty fixture checks same routing and intentionally fails against old library. Full Release/trace CTest required; complete real parent replay still mandatory.',
    limitation='Only a diagnostic continuation path; no policy, proof rule, Release byte, timing score or acceptance change.'), indent=2) + '\n')
print(json.dumps(binaries, indent=2))

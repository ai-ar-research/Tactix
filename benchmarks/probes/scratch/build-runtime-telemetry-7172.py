#!/usr/bin/env python3
"""Freeze a diagnostic-only telemetry ablation and independent unit-tested pins."""
from concurrent.futures import ThreadPoolExecutor
import difflib
import json
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
source = root / '.claude/worktrees/runtime-telemetry-ablation-7172'
parent = root / '.claude/worktrees/runtime-vivify-compact-7144'
c = root / 'benchmarks/runs/20260910-7172-runtime-telemetry-build'
assert not c.exists()
(c / 'logs').mkdir(parents=True)
files = {str(p.relative_to(source)): sha(p) for d in ('src', 'include', 'tests')
         for p in sorted((source / d).rglob('*')) if p.is_file()}
files['CMakeLists.txt'] = sha(source / 'CMakeLists.txt')
changed = {r for r in files if not (parent / r).exists() or sha(parent / r) != files[r]}
assert changed == {'CMakeLists.txt', 'src/kernel/kernel.cpp', 'src/rules/registry.cpp',
                   'src/policies/default_policy.cpp', 'tests/test_backbone.cpp',
                   'tests/test_runtime_telemetry.cpp'}, changed

def run(name, command):
    with (c / 'logs' / (name + '.log')).open('x') as out:
        p = subprocess.run(command, stdout=out, stderr=subprocess.STDOUT, cwd=c)
    assert p.returncode == 0, (name, p.returncode)
    print(name, 'PASS', flush=True)

def build(kind):
    out = root / ('build/pin-7172-runtime-telemetry-' + kind)
    assert not out.exists()
    run(kind + '-configure', ['cmake', '-S', str(source), '-B', str(out),
        '-DCMAKE_BUILD_TYPE=Release',
        '-DTACTIX_DISABLE_RUNTIME_TELEMETRY=' + ('OFF' if kind == 'control' else 'ON')])
    run(kind + '-build', ['cmake', '--build', str(out), '-j4'])
    run(kind + '-ctest', ['ctest', '--test-dir', str(out), '--output-on-failure', '-j1'])
    run(kind + '-fixture', [str(out / 'tests/test_runtime_telemetry')])
    return str((out / 'tactix').relative_to(root)), sha(out / 'tactix')

with ThreadPoolExecutor(max_workers=3) as pool:
    binaries = dict(pool.map(build, ('control', 'release', 'bc')))
assert binaries['build/pin-7172-runtime-telemetry-release/tactix'] == binaries['build/pin-7172-runtime-telemetry-bc/tactix']
assert binaries['build/pin-7172-runtime-telemetry-control/tactix'] == sha(root / 'build/pin-7162-vivify-revalidation-release/tactix'), 'enabled control must reproduce retained7144 byte for byte'
for rel, digest in files.items(): assert sha(source / rel) == digest
patches = {}
for name, base in (('incremental.patch', parent), ('cumulative.patch', root)):
    patch = ''
    for rel in sorted(files):
        a, b = base / rel, source / rel
        if a.exists() and sha(a) == sha(b): continue
        patch += ''.join(difflib.unified_diff(a.read_text().splitlines(True) if a.exists() else [],
            b.read_text().splitlines(True), fromfile='a/'+rel if a.exists() else '/dev/null', tofile='b/'+rel, n=0))
    with (c / name).open('x') as stream: stream.write(patch)
    patches[name] = sha(c / name)
with (c / 'manifest.json').open('x') as stream:
    json.dump(dict(source=str(source.relative_to(root)), parent=str(parent.relative_to(root)),
        source_commit='c2a95a6c79c21a76516cf556b6538e76da369c34', files=files,
        binaries=binaries, patches=patches, patch_application='git apply --unidiff-zero',
        hypothesis='Diagnostic-only joint ablation of binary-scan telemetry and per-rule clocks. Preserve Stats/Kernel layout, rule call counts, all logical budgets/meters, coarse whole-slot timing, traces/proofs/models and scheduling. No Factor change.',
        test_consideration='Both telemetry modes run full suites and four dispatch boundary/four binary-scan-state fixtures; existing Backbone early-stop, conflict, LRAT naming, cursor-debt and mixed-watch tests retain semantic assertions. Only telemetry expected values differ. Cross-arm certificate and all non-timing statistics except exactly the c binprop row must match.',
        status='Isolated unfuzzed diagnostic investigation, not production promotion; no changed defaults. Runtime hypothesis needs paired three-wave evidence; small gain will not be discarded solely for missing an arbitrary percentage.',
        logs={str(p.relative_to(c)): sha(p) for p in (c / 'logs').iterdir()}), stream, indent=2)
print(json.dumps(binaries, indent=2))

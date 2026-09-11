#!/usr/bin/env python3
"""Freeze and validate the complete single-image occurrence consumer cutover."""
from concurrent.futures import ThreadPoolExecutor
import difflib
import json
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
source = root / '.claude/worktrees/runtime-single-occurrence-cutover-7253'
parent = root / '.claude/worktrees/runtime-single-occurrence-7249'
c = root / 'benchmarks/runs/20260911-7253-occurrence-cutover-build'
assert not c.exists(), 'do not overwrite successful or failed evidence'
(c / 'logs').mkdir(parents=True)
files = {str(p.relative_to(source)): sha(p) for d in ('src', 'include', 'tests')
         for p in sorted((source / d).rglob('*')) if p.is_file()}
files['CMakeLists.txt'] = sha(source / 'CMakeLists.txt')
changed = {r for r in files if not (parent / r).exists() or sha(parent / r) != files[r]}
expected = {'src/diagnostics/event_trace.cpp', 'src/kernel/connected_occurrence.hpp',
    'src/kernel/kernel.cpp', 'src/kernel/kernel.hpp', 'src/policies/default_policy.cpp',
    'src/rules/bve/bve.cpp', 'src/rules/congruence/congruence.cpp',
    'src/rules/substitute/substitute.cpp', 'src/rules/subsume_forward/subsume_forward.cpp',
    'src/rules/sweep/sweep.cpp', 'src/rules/sweep/sweep.hpp', 'src/rules/ternary/ternary.cpp',
    'tests/occurrence_test_utils.hpp', 'tests/test_occurrence_cutover.cpp',
    'tests/test_connected_occurrence.cpp', 'tests/test_kernel.cpp', 'tests/test_bve.cpp',
    'tests/test_substitute.cpp', 'tests/test_collect_memory.cpp', 'tests/test_replacement_cursor.cpp'}
assert changed == expected, changed ^ expected
commands = {}
def run(label, command):
    command = list(map(str, command))
    p = subprocess.run(command, cwd=c, capture_output=True, text=True)
    with (c / 'logs' / (label + '.log')).open('x') as out:
        out.write(p.stdout + p.stderr)
    commands[label] = dict(command=command, exit_code=p.returncode)
    assert p.returncode == 0, (label, p.returncode, p.stdout, p.stderr)
    print(label, 'PASS', flush=True)
    return p.stdout

def build(kind):
    out = root / ('build/pin-7253-occurrence-cutover-' + kind)
    assert not out.exists()
    command = ['cmake', '-S', source, '-B', out, '-DCMAKE_BUILD_TYPE=Release',
        '-DCMAKE_C_COMPILER=/cm/local/apps/gcc/13.1.0/bin/gcc',
        '-DCMAKE_CXX_COMPILER=/cm/local/apps/gcc/13.1.0/bin/g++']
    if kind == 'trace': command += ['-DTACTIX_ENABLE_EVENT_TRACE=ON']
    run(kind + '-configure', command)
    run(kind + '-build', ['cmake', '--build', out, '-j4'])
    run(kind + '-ctest', ['ctest', '--test-dir', out, '--output-on-failure', '-j1'])
    for fixture in ('test_connected_occurrence', 'test_occurrence_cutover',
                    'test_replacement_cursor', 'test_kernel', 'test_bve',
                    'test_sweep', 'test_substitute', 'test_strengthen'):
        run(kind + '-' + fixture, [out / 'tests' / fixture])
    return str((out / 'tactix').relative_to(root)), sha(out / 'tactix')

with ThreadPoolExecutor(max_workers=3) as pool:
    binaries = dict(pool.map(build, ('release', 'bc', 'trace')))
assert binaries['build/pin-7253-occurrence-cutover-release/tactix'] == binaries['build/pin-7253-occurrence-cutover-bc/tactix']
objects = sorted((root / 'build/pin-7249-single-occurrence-release/CMakeFiles/tactix_lib.dir').rglob('*.o'))
assert objects
parent_exe = c / 'parent-boundary-fixture'
run('parent-boundary-build', ['/cm/local/apps/gcc/13.1.0/bin/g++', '-std=c++20', '-O3',
    '-DNDEBUG', '-fno-exceptions', '-fno-rtti', '-I' + str(parent / 'src'),
    '-I' + str(parent / 'include'), source / 'tests/test_occurrence_cutover.cpp',
    *objects, '-o', parent_exe])
oracle = run('parent-boundary', [parent_exe])
assert oracle == (c / 'logs/release-test_occurrence_cutover.log').read_text()
replacement = run('parent-replacement', [root / 'build/pin-7249-single-occurrence-release/tests/test_replacement_cursor'])
assert replacement == (c / 'logs/release-test_replacement_cursor.log').read_text()
for rel, digest in files.items(): assert sha(source / rel) == digest, rel
patches = {}
for name, base in (('incremental.patch', parent), ('cumulative.patch', root)):
    patch = ''
    for rel in sorted(files):
        a, b = base / rel, source / rel
        if a.exists() and sha(a) == sha(b): continue
        patch += ''.join(difflib.unified_diff(a.read_text().splitlines(True) if a.exists() else [],
            b.read_text().splitlines(True), fromfile='a/'+rel if a.exists() else '/dev/null', tofile='b/'+rel, n=0))
    with (c / name).open('x') as out: out.write(patch)
    patches[name] = sha(c / name)
with (c / 'manifest.json').open('x') as out:
    json.dump(dict(status='PASS', source=str(source.relative_to(root)),
        parent=str(parent.relative_to(root)), source_commit='c2a95a6c79c21a76516cf556b6538e76da369c34',
        files=files, changed=sorted(changed), binaries=binaries, commands=commands,
        patches=patches, patch_application='git apply --unidiff-zero',
        parent_objects={str(p.relative_to(root)):sha(p) for p in objects},
        oracle=oracle, replacement=replacement,
        hypothesis='One mixed owning payload plus exact large counts replaces Eliminate duplicate large payload. Probe remains contiguous and forward partial index remains distinct. No scheduling, budget, rule eligibility or proof change intended.',
        test_consideration='Exact old large-vs-mixed order asserted by same actual-parent/candidate fixture:52 cases364 boundaries across seven rule families, delayed binaries, duplicates, stale entries, in-place replacement, collection and mode reversal.840 replacement cases also exact. Storage adds retained-capacity rebuild and moved-owner controls. Full three suites and focused fixtures.',
        risk='Filtered traversal scans binary entries; whole-Eliminate/runtime/RSS repetitions decide net value. Physical relocation and memory diagnostics legitimately change. Fixed tests do not establish unrestricted equivalence, independent proof acceptance or fuzz coverage.',
        limitation='Isolated unpromoted investigation. Independent proof/model and sanitizer gates, BC, runtime/profile repetitions, full50 and Claude-owned fuzz still required. Retained7144 rollback and productionc2a95a6 unchanged.',
        logs={str(p.relative_to(c)):sha(p) for p in (c / 'logs').iterdir()}), out, indent=2)
print(json.dumps(binaries, indent=2))

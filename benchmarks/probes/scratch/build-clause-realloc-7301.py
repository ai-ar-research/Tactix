#!/usr/bin/env python3
"""Validate isolated clause-arena realloc against the actual7267 parent."""
from concurrent.futures import ThreadPoolExecutor
import difflib
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
source = root / '.claude/worktrees/runtime-clause-realloc-7301'
parent_manifest = root / 'benchmarks/runs/20260911-7271-dense-realloc-build/manifest.json'
pm = json.loads(parent_manifest.read_text())
parent = root / pm['source']
for rel, digest in pm['files'].items():
    assert sha(parent / rel) == digest, rel
c = root / 'benchmarks/runs/20260911-7301-clause-realloc-build'
assert not c.exists(), 'never overwrite successful or failed build evidence'
(c / 'logs').mkdir(parents=True)
files = {str(p.relative_to(source)): sha(p) for d in ('src', 'include', 'tests')
         for p in sorted((source / d).rglob('*')) if p.is_file()}
files['CMakeLists.txt'] = sha(source / 'CMakeLists.txt')
changed = {rel for rel, digest in files.items() if pm['files'].get(rel) != digest}
assert changed == {'src/kernel/clause.hpp', 'src/kernel/realloc_words.hpp',
                   'tests/CMakeLists.txt', 'tests/test_clause_arena_replay.cpp'}, changed
commands = {}
def run(label, command):
    command = list(map(str, command))
    p = subprocess.run(command, cwd=c, text=True, capture_output=True)
    with (c / 'logs' / (label + '.log')).open('x') as out:
        out.write(p.stdout + p.stderr)
    commands[label] = dict(command=command, exit_code=p.returncode)
    assert p.returncode == 0, (label, p.returncode, p.stdout[-1500:], p.stderr[-1500:])
    print(label, 'PASS', flush=True)
    return p.stdout

fixtures = ('test_clause_arena_replay', 'test_realloc_words', 'test_collect_memory',
            'test_occurrence_cutover', 'test_replacement_cursor', 'test_kernel',
            'test_bve', 'test_sweep', 'test_substitute', 'test_strengthen')
def build(kind):
    out = root / ('build/pin-7301-clause-realloc-' + kind)
    assert not out.exists()
    run(kind + '-configure', ['cmake', '-S', source, '-B', out,
        '-DCMAKE_BUILD_TYPE=Release', '-DTACTIX_ENABLE_RULE_JOURNAL=OFF',
        '-DTACTIX_ENABLE_PROOF_SHADOW=OFF', '-DTACTIX_ENABLE_CONFLICT_STOP=OFF',
        '-DTACTIX_ENABLE_EVENT_TRACE=' + ('ON' if kind == 'trace' else 'OFF'),
        '-DCMAKE_C_COMPILER=/cm/local/apps/gcc/13.1.0/bin/gcc',
        '-DCMAKE_CXX_COMPILER=/cm/local/apps/gcc/13.1.0/bin/g++'])
    run(kind + '-build', ['cmake', '--build', out, '-j4'])
    full = run(kind + '-ctest', ['ctest', '--test-dir', out, '--output-on-failure', '-j1'])
    assert '100% tests passed' in full and 'Skipped' not in full and '***Failed' not in full
    for fixture in fixtures:
        run(kind + '-' + fixture, [out / 'tests' / fixture])
    return str((out / 'tactix').relative_to(root)), sha(out / 'tactix')

with ThreadPoolExecutor(max_workers=3) as pool:
    binaries = dict(pool.map(build, ('release', 'bc', 'trace')))
assert binaries['build/pin-7301-clause-realloc-release/tactix'] == binaries['build/pin-7301-clause-realloc-bc/tactix']
parent_out = root / 'build/pin-7267-dense-realloc-release'
objects = sorted((parent_out / 'CMakeFiles/tactix_lib.dir').rglob('*.o'))
assert objects
oracle = {}
for fixture in ('test_clause_arena_replay', 'test_occurrence_cutover'):
    executable = c / ('parent-' + fixture)
    run(fixture + '-parent-build', ['/cm/local/apps/gcc/13.1.0/bin/g++', '-std=c++20',
        '-O3', '-DNDEBUG', '-fno-exceptions', '-fno-rtti', '-I' + str(parent / 'src'),
        '-I' + str(parent / 'include'), source / 'tests' / (fixture + '.cpp'), *objects, '-o', executable])
    oracle[fixture] = run(fixture + '-parent', [executable])
    for kind in ('release', 'bc', 'trace'):
        assert oracle[fixture] == (c / 'logs' / (kind + '-' + fixture + '.log')).read_text()
replacement = run('parent-replacement', [parent_out / 'tests/test_replacement_cursor'])
assert replacement == (c / 'logs/release-test_replacement_cursor.log').read_text()
patches = {}
for name, base in (('incremental.patch', parent), ('cumulative.patch', root)):
    patch = ''
    for rel in sorted(files):
        a, b = base / rel, source / rel
        if a.exists() and sha(a) == sha(b):
            continue
        patch += ''.join(difflib.unified_diff(a.read_text().splitlines(True) if a.exists() else [],
            b.read_text().splitlines(True), fromfile='a/' + rel if a.exists() else '/dev/null',
            tofile='b/' + rel, n=0))
    with (c / name).open('x') as out:
        out.write(patch)
    patches[name] = sha(c / name)
for rel, digest in files.items():
    assert sha(source / rel) == digest, rel
with (c / 'manifest.json').open('x') as out:
    json.dump(dict(status='PASS', source=str(source.relative_to(root)), parent=str(parent.relative_to(root)),
        source_commit=pm['source_commit'], files=files, changed=sorted(changed), binaries=binaries,
        parent_manifest=dict(path=str(parent_manifest.relative_to(root)), sha256=sha(parent_manifest)),
        parent_objects={str(p.relative_to(root)): sha(p) for p in objects},
        commands=commands, oracle=oracle, replacement=replacement, patches=patches,
        patch_application='git apply --unidiff-zero',
        logs={str(p.relative_to(c)): sha(p) for p in (c / 'logs').iterdir()},
        helper_sha256=sha(Path(__file__)),
        hypothesis='Exact-Release syscall evidence7295/7297 shows later clause-arena vector growth retaining simultaneous old/new buffers over a large heap. Apply the existing checked realloc word owner to Arena, preserving vector growth sizes, initialized values, Ref offsets, collection and all rule/strategy/proof behavior. Test removal of transient copying, not an assumed global allocator/RSS fix.',
        test_consideration='New32768-clause32-collection sparse/dense growth-capacity/raw-word/id/copy/move replay compiled against actual old/new libraries; existing35815 allocator operations with forced moves/failure,52-case364-boundary rule replay and840 replacements. Three full suites; then unsuppressed sanitizers and actual-parent proof/model controls before BC/runtime.',
        limitation='Unpromoted investigation; collection remains copying. Realloc can move or fail and generated code/allocator behavior can regress. No RSS/runtime gain, completed pending proof, full50, fuzz or consolidation acceptance inferred.'), out, indent=2)
print(c / 'manifest.json')

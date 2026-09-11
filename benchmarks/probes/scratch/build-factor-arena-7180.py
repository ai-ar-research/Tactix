#!/usr/bin/env python3
"""Build isolated Factor occurrence arenas without changing accepted pins."""
from concurrent.futures import ThreadPoolExecutor
import difflib
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
source = root / '.claude/worktrees/runtime-factor-arena-7180'
parent = root / '.claude/worktrees/runtime-vivify-compact-7144'
c = root / 'benchmarks/runs/20260910-7180-factor-arena-build'
assert not c.exists()
(c / 'logs').mkdir(parents=True)
files = {str(p.relative_to(source)): sha(p) for d in ('src', 'include', 'tests')
         for p in sorted((source / d).rglob('*')) if p.is_file()}
files['CMakeLists.txt'] = sha(source / 'CMakeLists.txt')
changed = {r for r in files if not (parent / r).exists() or sha(parent / r) != files[r]}
assert changed == {'src/rules/factor/factor.cpp', 'src/rules/factor/occurrence_arena.hpp',
                   'tests/test_factor_occurrence_arena.cpp'}, changed
def run(name, command):
    with (c / 'logs' / (name + '.log')).open('x') as out:
        p = subprocess.run(command, cwd=c, stdout=out, stderr=subprocess.STDOUT)
    assert p.returncode == 0, (name, p.returncode)
    print(name, 'PASS', flush=True)
def build(kind):
    out = root / ('build/pin-7180-factor-arena-' + kind)
    assert not out.exists()
    command = ['cmake', '-S', str(source), '-B', str(out), '-DCMAKE_BUILD_TYPE=Release']
    if kind == 'trace': command += ['-DTACTIX_ENABLE_EVENT_TRACE=ON']
    run(kind + '-configure', command)
    run(kind + '-build', ['cmake', '--build', str(out), '-j4'])
    run(kind + '-ctest', ['ctest', '--test-dir', str(out), '--output-on-failure', '-j1'])
    for fixture in ('test_factor_occurrence_arena', 'test_factor_root_support', 'test_factor', 'test_factor_n1'):
        run(kind + '-' + fixture, [str(out / 'tests' / fixture)])
    return str((out / 'tactix').relative_to(root)), sha(out / 'tactix')
with ThreadPoolExecutor(max_workers=3) as pool:
    binaries = dict(pool.map(build, ('release', 'bc', 'trace')))
assert binaries['build/pin-7180-factor-arena-release/tactix'] == binaries['build/pin-7180-factor-arena-bc/tactix']
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
    json.dump(dict(source=str(source.relative_to(root)), parent=str(parent.relative_to(root)),
        source_commit='c2a95a6c79c21a76516cf556b6538e76da369c34', files=files,
        binaries=binaries, patches=patches, patch_application='git apply --unidiff-zero',
        hypothesis='Replace independent per-literal Factor allocations with one eight-byte-record arena. Reuse final peeling counts as initial capacity upper bounds, preserving final connection eligibility and every live encounter order. Dynamic append relocation and stable tail erasure retain caches/degree/tick/proof contracts; size_t offsets avoid a new aggregate32-bit limit. No cofactor-loop rewrite or telemetry ablation included.',
        test_consideration='New34 storage layouts compare every operation against vector-of-vectors: mixed growth, cached-ID mutation, stable duplicate deletion, empty erase, reset and fresh literals. Existing Factor support, N1, grid/tick/pose/extension/model tests run in all three suites. Independent exact-parent proofs, DebugASan/UBSan/LSan, BC and complete-Factor/whole-runtime/RSS checks still required.',
        risk='Arena growth invalidates iterators globally; no caller may retain one across any append. Initial upper-bound capacity and abandoned grown slices may inflate RSS; measure, do not assume a memory win. Binary-only scope performs no large-index scan. Kernel/global FlatLists contract unchanged.',
        status='Isolated unfuzzed investigation; retained7144 rollback. No production promotion, changed scheduling, fuzz waiver, old-pin mutation or acceptance before tests and matched performance evidence.',
        logs={str(p.relative_to(c)): sha(p) for p in (c / 'logs').iterdir()}), out, indent=2)
print(json.dumps(binaries, indent=2))

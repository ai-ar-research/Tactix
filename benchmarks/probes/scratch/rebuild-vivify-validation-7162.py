#!/usr/bin/env python3
"""Build independent Release, B/C and trace pins for compact Vivify7144."""
from concurrent.futures import ThreadPoolExecutor
import difflib
import json
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
source = root / '.claude/worktrees/runtime-vivify-compact-7144'
parent = root / '.claude/worktrees/runtime-collect-retry-7030'
c = root / 'benchmarks/runs/20260910-7162-vivify-revalidation-build'
assert not c.exists()
(c / 'logs').mkdir(parents=True)
original_manifest = json.loads((root / 'benchmarks/runs/20260910-7144-vivify-compact-build/manifest.json').read_text())
files = {str(p.relative_to(source)): sha(p) for d in ('src', 'include', 'tests')
         for p in sorted((source / d).rglob('*')) if p.is_file()}
files['CMakeLists.txt'] = sha(source / 'CMakeLists.txt')
changed = {r for r in files if not (parent / r).exists() or sha(parent / r) != files[r]}
assert changed == {'src/rules/vivify/vivify.cpp', 'tests/test_vivify_order_contract.cpp',
                   'tests/test_vivify_compact_contract.cpp'}, changed

def run(name, command, cwd=None):
    with (c / 'logs' / (name + '.log')).open('x') as out:
        p = subprocess.run(command, cwd=cwd, stdout=out, stderr=subprocess.STDOUT)
    assert p.returncode == 0, (name, p.returncode)
    print(name, 'PASS', flush=True)

def build(kind):
    out = root / ('build/pin-7162-vivify-revalidation-' + kind)
    assert not out.exists()
    command = ['cmake', '-S', str(source), '-B', str(out), '-DCMAKE_BUILD_TYPE=Release']
    if kind == 'trace': command += ['-DTACTIX_ENABLE_EVENT_TRACE=ON']
    run(kind + '-configure', command)
    run(kind + '-build', ['cmake', '--build', str(out), '-j4'])
    run(kind + '-ctest', ['ctest', '--test-dir', str(out), '--output-on-failure', '-j1'])
    for fixture in ('test_vivify_order_contract', 'test_vivify_compact_contract', 'test_vivify_count_cache'):
        run(kind + '-' + fixture, [str(out / 'tests' / fixture)], cwd=c)
    return str((out / 'tactix').relative_to(root)), sha(out / 'tactix')

with ThreadPoolExecutor(max_workers=3) as pool:
    binaries = dict(pool.map(build, ('release', 'bc', 'trace')))
assert binaries['build/pin-7162-vivify-revalidation-release/tactix'] == binaries['build/pin-7162-vivify-revalidation-bc/tactix']
for rel, digest in files.items(): assert sha(source / rel) == digest == original_manifest['files'][rel], rel
for kind in ('release', 'bc', 'trace'):
    assert binaries[f'build/pin-7162-vivify-revalidation-{kind}/tactix'] == original_manifest['binaries'][f'build/pin-7144-vivify-compact-{kind}/tactix'], kind
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
        source_commit='c2a95a6c79c21a76516cf556b6538e76da369c34', production='c2a95a6c79c21a76516cf556b6538e76da369c34',
        files=files, binaries=binaries, patches=patches, patch_application='git apply --unidiff-zero',
        hypothesis='Compact homogeneous canonical Vivify: arena redundant poses/ref sorting and ordinary irredundant stable count radix; preserve generic fallbacks and exact transactions.',
        test_consideration='Existing full Vivify suites plus96 ordering cases and1728 transaction/model/proof-digest cases across priorities, scopes, root-false rewrites, imports, relocation, reuse and flush. Frozen-parent comparison, sanitizer, independent proof and runtime gates remain required.',
        status='User-requested fresh validation of retained7144 before any further integration commit; exact original source and binary hashes required. Current-source fuzz and full50 independent checking remain open.',
        logs={str(p.relative_to(c)): sha(p) for p in (c / 'logs').iterdir()}), stream, indent=2)
print(json.dumps(binaries, indent=2))

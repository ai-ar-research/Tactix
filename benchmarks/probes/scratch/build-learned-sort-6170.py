#!/usr/bin/env python3
"""Build independent Release pins and seal the isolated private-sort change."""
from concurrent.futures import ThreadPoolExecutor
import difflib
import json
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
source = root / '.claude/worktrees/runtime-learned-sort-6169'
parent = root / '.claude/worktrees/runtime-bve-capacity-6154'
c = root / 'benchmarks/runs/20260909-6169-learned-sort-build'
assert not (c / 'manifest.json').exists()

def run(name, command):
    with (c / 'logs' / (name + '.log')).open('x') as out:
        p = subprocess.run(command, stdout=out, stderr=subprocess.STDOUT)
    assert p.returncode == 0, (name, p.returncode)
    print(name, 'PASS', flush=True)

def build(number, kind):
    out = root / f'build/pin-{number}-learned-sort-{kind}'
    assert not out.exists()
    run(kind + '-configure', ['cmake', '-S', str(source), '-B', str(out), '-DCMAKE_BUILD_TYPE=Release'])
    run(kind + '-build', ['cmake', '--build', str(out), '-j6'])
    run(kind + '-ctest', ['ctest', '--test-dir', str(out), '--output-on-failure', '-j1'])
    return str((out / 'tactix').relative_to(root)), sha(out / 'tactix')

with ThreadPoolExecutor(max_workers=2) as pool:
    results = list(pool.map(lambda args: build(*args), [(6169, 'release'), (6170, 'bc')]))
assert results[0][1] == results[1][1], 'independent Release identity'
inventory = {str(p.relative_to(source)): sha(p) for d in ('src', 'include', 'tests')
             for p in sorted((source / d).rglob('*')) if p.is_file()}
inventory['CMakeLists.txt'] = sha(source / 'CMakeLists.txt')
patches = {}
for name, base in (('incremental.patch', parent), ('cumulative.patch', root)):
    patch = ''
    for rel in sorted(inventory):
        a, b = base / rel, source / rel
        if a.exists() and sha(a) == sha(b):
            continue
        patch += ''.join(difflib.unified_diff(a.read_text().splitlines(True) if a.exists() else [],
            b.read_text().splitlines(True), fromfile='a/' + rel if a.exists() else '/dev/null',
            tofile='b/' + rel, n=0))
    (c / name).write_text(patch)
    patches[name] = sha(c / name)
changed = [r for r in inventory if not (parent / r).exists() or sha(parent / r) != inventory[r]]
assert set(changed) == {'src/rules/analyze/analyze.cpp', 'src/rules/analyze/learned_sort.hpp', 'tests/test_learned_sort.cpp'}
manifest = dict(source=str(source.relative_to(root)), parent=str(parent.relative_to(root)),
    source_commit='c2a95a6c79c21a76516cf556b6538e76da369c34', binaries=dict(results),
    files=inventory, patches=patches, patch_application='git apply --unidiff-zero',
    profile_authority='benchmarks/runs/20260909-6166-rank4-release-profile/summary.json',
    hypothesis='Omit stable identity radix passes on constant bytes, preserving learned clause order exactly; no policy, proof, root-closure or memory-layout change.',
    test_consideration='433 fixed comparison-sort cases cover all16 active-byte masks, odd/even scatter parity, stable ties, UIP preservation, 32-bit boundaries and scratch reuse; full independent Release suites. Existing M3 API/model/proof tests cover unchanged semantics.',
    acceptance='Exact full non-timing work and proof replay; checked B28+C5 then targeted Rank4 three paired unprofiled waves and full50 fixed reference. Claude-owned fuzz is required before promotion.',
    status='Isolated unfuzzed investigation material; no speedup or promotion established.',
    memory_design='Separate realloc-backed FlatLists proposal remains unapproved and unimplemented.',
    logs={str(p.relative_to(c)): sha(p) for p in (c / 'logs').iterdir()})
(c / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
print(json.dumps(dict(binaries=dict(results), changed=changed), indent=2), flush=True)

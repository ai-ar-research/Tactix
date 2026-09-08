#!/usr/bin/env python3
"""Freeze the general root-closure repair and independent tested Release pins."""
import difflib
import hashlib
import json
from pathlib import Path

root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260907-5417-rephase-root'
source = root / '.claude/worktrees/runtime-rephase-root-5417'
parent = root / '.claude/worktrees/runtime-imply-fast-5414'
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
for name, count in [('trace', 76), ('release', 77), ('bc', 77)]:
    assert f'100% tests passed out of {count}' in (c / 'logs' / f'ctest-{name}.log').read_text()
files = ['src/policies/default_policy.cpp', 'src/policies/default_policy.hpp', 'tests/test_rephase.cpp']
def patch(before, paths):
    return ''.join(''.join(difflib.unified_diff((before / f).read_text().splitlines(True), (source / f).read_text().splitlines(True), fromfile='a/' + f, tofile='b/' + f)) for f in paths)
for name, body in [('repair.patch', patch(parent, files)), ('cumulative.patch', patch(root, [str(p.relative_to(root)) for p in sorted((root / 'src').rglob('*')) if p.is_file()]))]:
    assert not (c / name).exists()
    (c / name).write_text(body)
binaries = ['build/pin-5417-rephase-root-trace/tactix', 'build/pin-5419-rephase-root-release/tactix', 'build/pin-5420-rephase-root-bc/tactix']
assert sha(root / binaries[1]) == sha(root / binaries[2])
m = dict(purpose='Restore the root propagation fixpoint before DefaultPolicy Rephase/Walk warm-up, using existing Restart and Propagate operations.',
         source=str(source.relative_to(root)), parent=str(parent.relative_to(root)), production='c2a95a6c79c21a76516cf556b6538e76da369c34',
         references=['Kissat rephase.c:kissat_rephase and backtrack.c:kissat_backtrack_propagate_and_flush_trail', 'CaDiCaL walk.cpp:Internal::walk backtracks and propagates root before walking'],
         semantics='No public rule/guard/proof-vocabulary change. Policy helper restores retained-root scans before phase rewrites; root conflicts use existing checked Propagate proof path. Public Rephase remains H-only and usable mid-trail.',
         fixture='Actual old composition yields implication order -2,-4,-5 after a stale transient blocker; new policy helper yields -4,-5,-2, root fixpoint and unchanged canonical restart count. Both paths execute successfully; no crashing or unsoundness reproducer.',
         status='UNFUZZED isolated candidate, no promotion or runtime acceptance',
         files={f: sha(source / f) for f in files}, binaries={p: sha(root / p) for p in binaries},
         patches={p.name: sha(p) for p in c.glob('*.patch')}, logs={p.name: sha(p) for p in (c / 'logs').iterdir()})
assert not (c / 'manifest.json').exists()
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
print('PASS: trace 76/76, independent Release 77/77 each; equal Release hashes; source frozen')

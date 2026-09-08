#!/usr/bin/env python3
"""Freeze exact Sweep-ledger membership behavior and independent tested pins."""
from pathlib import Path
import difflib
import hashlib
import json
root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260908-5473-sweep-cursor'
source = root / '.claude/worktrees/runtime-sweep-cursor-5473'
parent = root / '.claude/worktrees/runtime-watch-pair-5466'
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
fixture = 'sweep-cursor cases=588 state=b15f97a272f9abef'
assert (c / 'logs/old-fixture.log').read_text().strip() == fixture
builds = ['build/pin-5473-sweep-cursor-release', 'build/pin-5474-sweep-cursor-bc']
for arm, b in zip(('release', 'bc'), builds):
    assert '100% tests passed out of 84' in (c / 'logs' / f'ctest-{arm}.log').read_text()
    assert fixture in (root / b / 'Testing/Temporary/LastTest.log').read_text()
assert sha(root / builds[0] / 'tactix') == sha(root / builds[1] / 'tactix')
files = ['src/rules/substitute/substitute.cpp', 'tests/test_sweep_cursor.cpp']
def patch(before, names):
    return ''.join(''.join(difflib.unified_diff(
        (before / n).read_text().splitlines(True) if (before / n).exists() else [],
        (source / n).read_text().splitlines(True),
        fromfile='a/' + n if (before / n).exists() else '/dev/null', tofile='b/' + n))
        for n in names)
for name, body in [('repair.patch', patch(parent, files)), ('cumulative.patch', patch(root,
    [str(p.relative_to(root)) for p in sorted((root / 'src').rglob('*')) if p.is_file()]))]:
    assert not (c / name).exists()
    (c / name).write_text(body)
m = dict(source=str(source.relative_to(root)), parent=str(parent.relative_to(root)),
    production='c2a95a6c79c21a76516cf556b6538e76da369c34',
    purpose='Avoid repeated retained-prefix membership scans in the private Sweep mixed-occurrence ledger.',
    motivation='Clean 5425 profile: Sweep rewrite_large_in_place has 1.02% self samples; 5472 annotation places nearly all 727 local samples in its two full-range membership loops. Not a measured removable runtime fraction.',
    references=['kissat/src/sweep.c::substitute_connected_clauses preserves ordered survivor runs and appends moved occurrences',
                'cadical/src/sweep.cpp::substitute_connected_clauses constructs the image in source order'],
    contract='For each boolean membership query search [next,end) then [begin,next); advance only after a hit. Exact full-range membership for arbitrary images, no shared marks, allocation, update order, representation, public primitive, rule guard/effect, proof, schedule or effort change. Ordered hits avoid repeated prefixes; adversarial misses/reordering may remain quadratic.',
    fixture='588 actual old/new bookkeeping cases: seven clause lengths, seven image shapes, both clause kinds, sparse/dense/ordered-dense, direct and delayed-transaction modes. Assert image occurrence membership, unchanged arena and matching full ledger order/capacity/dirty-state digest b15f97a272f9abef after move flush. Existing substitute/Sweep API and proof tests remain in full suites.',
    status='Isolated UNFUZZED candidate on 5466; excludes independent metadata 5426. Fuzz owner 5442 covers only 5438. No runtime gain or promotion claimed.',
    files={n: sha(source / n) for n in files},
    binaries={b + '/tactix': sha(root / b / 'tactix') for b in builds},
    patches={p.name: sha(p) for p in c.glob('*.patch')},
    logs={p.name: sha(p) for p in (c / 'logs').iterdir()})
assert not (c / 'manifest.json').exists()
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
print(json.dumps(m['binaries'], indent=2))

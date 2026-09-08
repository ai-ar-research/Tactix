#!/usr/bin/env python3
"""Freeze tested representation-neutral replacement membership lookup."""
import difflib
import hashlib
import json
from pathlib import Path

root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260907-5445-replacement-cursor'
source = root / '.claude/worktrees/runtime-replacement-cursor-5445'
parent = root / '.claude/worktrees/runtime-vivify-max-5438'
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
fixture = 'replacement-cursor cases=840 state=f86da0be45055db3'
assert (c/'logs/old-fixture.log').read_text().strip() == fixture
builds = ['build/pin-5445-replacement-cursor-release', 'build/pin-5446-replacement-cursor-bc']
for arm, build in zip(('release', 'bc'), builds):
    assert '100% tests passed out of 80' in (c/'logs'/f'ctest-{arm}.log').read_text()
    assert fixture in (root/build/'Testing/Temporary/LastTest.log').read_text()
assert sha(root/builds[0]/'tactix') == sha(root/builds[1]/'tactix')
files = ['src/kernel/kernel.cpp', 'tests/test_replacement_cursor.cpp']
def patch(before, names):
    result = ''
    for name in names:
        a, b = before/name, source/name
        result += ''.join(difflib.unified_diff(a.read_text().splitlines(True) if a.exists() else [],
            b.read_text().splitlines(True), fromfile='a/'+name if a.exists() else '/dev/null', tofile='b/'+name))
    return result
for name, body in [('repair.patch', patch(parent, files)), ('cumulative.patch', patch(root,
    [str(p.relative_to(root)) for p in sorted((root/'src').rglob('*')) if p.is_file()]))]:
    assert not (c/name).exists()
    (c/name).write_text(body)
m = dict(purpose='Avoid repeated full-prefix literal scans during in-place large-clause replacement using exact wraparound search cursors.',
    source=str(source.relative_to(root)), parent=str(parent.relative_to(root)),
    production='c2a95a6c79c21a76516cf556b6538e76da369c34',
    motivation='Clean 5425 profile, annotated in 5444: 1313 replacement samples, with almost all local periods in the three membership-search loops. No measured candidate speedup yet.',
    references=['Kissat sweep.c:substitute_connected_clauses retains unchanged literal runs during one-pass image construction',
        'CaDiCaL subsume.cpp:strengthen_clause preserves survivor order via remove and shrink'],
    semantics='Each query searches [next,end), then [begin,next), so its boolean is exactly full-range membership for arbitrary order. Only the successful-search cursor advances; no shared state, allocation, sorting, guard, API, proof, update order, timer or cadence change. Worst-case quadratic membership remains for adversarial reorderings/misses; ordered hits avoid repeated prefixes.',
    fixture='840 fixed actual old/new transition cases, two replacements per case; identical state f86da0be45055db3. Five lengths, seven image shapes including misses/reversal/rotation, both clause kinds, sparse/dense/ordered-dense, added-material on/off and retained watch on/off. Arena/span, physical watches, occurrence order/counts, advice and callback order checked.',
    status='Isolated UNFUZZED candidate. Includes 5438, excludes 5426. Ongoing fuzz 5442 covers predecessor 5438, not this change. No runtime acceptance or promotion.',
    files={name:sha(source/name) for name in files},
    binaries={build+'/tactix':sha(root/build/'tactix') for build in builds},
    patches={p.name:sha(p) for p in c.glob('*.patch')},
    logs={p.name:sha(p) for p in (c/'logs').iterdir()})
assert not (c/'manifest.json').exists()
(c/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
print('PASS: actual old/new 840 state cases; independent 80/80 tests and identical Release binaries')

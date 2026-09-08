#!/usr/bin/env python3
"""Freeze the exact-rank optimization after old/new fixture and full tests."""
import difflib
import hashlib
import json
from pathlib import Path

root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260907-5438-vivify-max'
source = root / '.claude/worktrees/runtime-vivify-max-5438'
parent = root / '.claude/worktrees/runtime-inprocess-root-5430'
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
for arm in ('release', 'bc'):
    assert '100% tests passed out of 79' in (c / 'logs' / ('ctest-' + arm + '.log')).read_text()
fixture = (c / 'logs/old-fixture.log').read_text().strip()
assert fixture == 'vivify-max cases=144 order-and-pose=a223a06d79fb69dd'
builds = ['build/pin-5438-vivify-max-release', 'build/pin-5439-vivify-max-bc']
for build in builds:
    assert fixture in (root / build / 'Testing/Temporary/LastTest.log').read_text()
assert sha(root / builds[0] / 'tactix') == sha(root / builds[1] / 'tactix')
files = ['src/rules/vivify/vivify.cpp', 'tests/test_vivify_max.cpp']
def patch(before, paths):
    result = ''
    for name in paths:
        a, b = before / name, source / name
        result += ''.join(difflib.unified_diff(a.read_text().splitlines(True) if a.exists() else [],
            b.read_text().splitlines(True), fromfile='a/' + name if a.exists() else '/dev/null', tofile='b/' + name))
    return result
for name, body in [('repair.patch', patch(parent, files)), ('cumulative.patch', patch(root,
        [str(p.relative_to(root)) for p in sorted((root / 'src').rglob('*')) if p.is_file()]))]:
    assert not (c / name).exists(); (c / name).write_text(body)
m = dict(purpose='Replace unnecessary whole-snapshot literal sorting with linear maximum-count selection in the pure irredundant, non-flushing Vivify schedule.',
    source=str(source.relative_to(root)), parent=str(parent.relative_to(root)),
    production='c2a95a6c79c21a76516cf556b6538e76da369c34',
    motivation='Clean profile 5425 samples Vivify scheduling at 1.66% plus candidate sorting at 0.98%; this bounds a cost investigation, not measured benefit of this specific branch.',
    references=['Kissat vivify.c:init_countref, COUNTREF_COUNTS == 1, and sort_vivification_candidates',
        'CaDiCaL vivify.cpp global literal order; retain the full lexicographic path for mixed and prefix-flushing scopes'],
    semantics='Same candidates, priorities, maximum count, size and position ties. Private irredundant keys are neither committed to clause pose nor inspected as prefixes when flushing is disabled. Mixed scopes, precise redundant ordering, prefix flushing and the existing no-sort regime are unchanged. No guard, public API, proof vocabulary, timer, allowance or cadence change; calculus/production ledger unchanged.',
    fixture='144 fixed actual old/new public scheduler cases; identical order-and-pose digest a223a06d79fb69dd. Covers all six scopes, reuse/flush on/off, three priority patterns, both sides of existing no-sort condition, repeatability and nonmutation; checks exact maximum-count rank in the changed path.',
    status='Isolated UNFUZZED performance investigation. Metadata 5426 excluded; no runtime acceptance or promotion.',
    files={name: sha(source / name) for name in files},
    binaries={build + '/tactix': sha(root / build / 'tactix') for build in builds},
    patches={p.name: sha(p) for p in c.glob('*.patch')},
    logs={p.name: sha(p) for p in (c / 'logs').iterdir()})
assert not (c / 'manifest.json').exists()
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
print('PASS: old/new 144 cases identical; independent 79/79 tests and matching Release hashes')

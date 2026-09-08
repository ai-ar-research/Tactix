#!/usr/bin/env python3
"""Freeze the tested phase-owned resolution stop and reviewed unit fixture."""
import difflib
import hashlib
import json
from pathlib import Path

root=Path(__file__).resolve().parents[3]
c=root/'benchmarks/runs/20260907-5450-eliminate-limit'
source=root/'.claude/worktrees/runtime-eliminate-limit-5450'
parent=root/'.claude/worktrees/runtime-replacement-cursor-5445'
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
builds={arm:root/f'build/pin-{number}-eliminate-limit-{arm}'
        for arm,number in [('release',5450),('trace',5451),('bc',5452)]}
for arm,b in builds.items():
    expected=80 if arm=='trace' else 81
    assert f'100% tests passed out of {expected}' in (c/'logs'/f'ctest-{arm}.log').read_text()
    assert 'eliminate-limit: 12 fixed phase scenarios passed' in (b/'Testing/Temporary/LastTest.log').read_text()
assert sha(builds['release']/'tactix')==sha(builds['bc']/'tactix')
files=['src/policies/default_inprocess.hpp','src/policies/default_policy.cpp','tests/test_eliminate_limit.cpp']
def patch(before,names):
    return ''.join(''.join(difflib.unified_diff(
        (before/n).read_text().splitlines(True) if (before/n).exists() else [],
        (source/n).read_text().splitlines(True),fromfile='a/'+n if (before/n).exists() else '/dev/null',tofile='b/'+n)) for n in names)
for name,body in [('repair.patch',patch(parent,files)),('cumulative.patch',patch(root,
    [str(p.relative_to(root)) for p in sorted((root/'src').rglob('*')) if p.is_file()]))]:
    assert not (c/name).exists()
    (c/name).write_text(body)
m=dict(source=str(source.relative_to(root)),parent=str(parent.relative_to(root)),
    production='c2a95a6c79c21a76516cf556b6538e76da369c34',
    purpose='Stop optional outer Eliminate rounds after BVE exceeds the existing shared resolution ceiling; retain current-round cleanup, strict equality and fresh-phase budget derivation.',
    references=['kissat/src/eliminate.c::eliminate_variables, post-round strict resolution-limit check',
        'cadical/src/elim.cpp::elim, incomplete elim_round stops before the next subsume/block/cover round'],
    evidence='5449 confirms 12587157 > 12587131 before the extra Rank-37 Strengthen at conflict 29761; 5436 owns normalized boundary comparison.',
    contract='Private phase-local scheduler latch only. No public rule/guard/API/proof/reconstruction change, new global primitive, effort constant or formula-specific gate. Mandatory first/current-round and phase-exit cleanup remain.',
    fixture='12 actual scheduler scenarios, two phase invocations each: resolution allowances 3/4/5 for four real pivot resolutions, Strengthen off/on, BVE off/on. Old strict-exhaustion case offers two rounds, new one; equality retains zero-cost pure eliminations and its next round. Disabled BVE, fresh ceiling, public guards, live clauses, root fixpoint and invariants checked.',
    fixture_review=dict(status='PASS, fixture-only correction; no solver defect',
        response='benchmarks/probes/scratch/claude-eliminate-fixture-5450-response.json',
        details='The original one-resolution fixture incorrectly expected one elimination at equality; native-compatible pure eliminations spend no additional resolutions. Bounded Claude review corrected the fixture and passed 49/49 predecessor in-process units. The deliberate future-expectation negative fails only at the intended round-count assertion. Original failure logs retained.'),
    status='Isolated UNFUZZED cumulative candidate; parent 5445 plus 5438, excludes independent metadata 5426. Background fuzz 5442 covers only 5438. No runtime claim or promotion.',
    files={n:sha(source/n) for n in files},
    binaries={str(b.relative_to(root))+'/tactix':sha(b/'tactix') for b in builds.values()},
    patches={p.name:sha(p) for p in c.glob('*.patch')},
    logs={p.name:sha(p) for p in (c/'logs').iterdir()})
assert not (c/'manifest.json').exists()
(c/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
print(json.dumps(m['binaries'],indent=2))

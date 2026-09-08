#!/usr/bin/env python3
"""Freeze tested bound advancement; preserve source, patch and test identities."""
import difflib
import hashlib
import json
from pathlib import Path

root=Path(__file__).resolve().parents[3]
c=root/'benchmarks/runs/20260907-5459-bound-advance'
source=root/'.claude/worktrees/runtime-bound-advance-5459'
parent=root/'.claude/worktrees/runtime-eliminate-limit-5450'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
builds={arm:root/f'build/pin-{number}-bound-advance-{arm}'
        for arm,number in [('release',5459),('trace',5460),('bc',5461)]}
for arm,b in builds.items():
    expected=81 if arm=='trace' else 82
    assert f'100% tests passed out of {expected}' in (c/'logs'/f'ctest-{arm}.log').read_text()
    assert 'BVE cut-bound lifecycle: PASS' in (b/'Testing/Temporary/LastTest.log').read_text()
assert sha(builds['release']/'tactix')==sha(builds['bc']/'tactix')
files=['src/policies/default_inprocess.hpp','src/policies/default_policy.cpp',
       'tests/test_slot_pose.cpp','tests/test_eliminate_limit.cpp','tests/test_bve_bound_cut.cpp']
def patch(before,names):
    return ''.join(''.join(difflib.unified_diff(
        (before/n).read_text().splitlines(True) if (before/n).exists() else [],
        (source/n).read_text().splitlines(True),
        fromfile='a/'+n if (before/n).exists() else '/dev/null',tofile='b/'+n)) for n in names)
for name,body in [('repair.patch',patch(parent,files)),('cumulative.patch',patch(root,
        [str(p.relative_to(root)) for p in sorted((root/'src').rglob('*')) if p.is_file()]))]:
    assert not (c/name).exists();(c/name).write_text(body)
m=dict(source=str(source.relative_to(root)),parent=str(parent.relative_to(root)),
    production='c2a95a6c79c21a76516cf556b6538e76da369c34',
    purpose='Restore SP-FACTOR-1: every consistent engaged Eliminate phase advances its bound, even on a quota cut; terminal phases preserve unvisited retry flags.',
    references=['kissat/src/eliminate.c::eliminate_variables releases its schedule before final remain; outer last_round_eliminated stays zero while round-local counter shadows it',
        'kissat/src/factor.c snapshots bounds.eliminate.additional_clauses; accepts reduction strictly greater than bound',
        'cadical/src/elim.cpp requires actual round completion; Kissat is the repository tie-break and previously documented contract'],
    contract='Private scheduling state only. Retains 5450 strict phase-resolution stop, all rule guards, instances, proof and reconstruction, effort constants and budgets. No formula-specific selectors.',
    fixture='Two actual old/new cut-bound cases: irregular advancing ladder and terminal bound, retry flags, admission watermark, coupled Factor bound, root fixpoint and public BVE guard. Updates existing scheduler and 12 phase-budget scenarios to the restored next-phase retry lifecycle.',
    status='Isolated UNFUZZED candidate. Background 5442 covers only 5438, not this candidate or 5445/5450. No runtime gain or promotion claimed.',
    files={n:sha(source/n) for n in files},
    binaries={str(b.relative_to(root))+'/tactix':sha(b/'tactix') for b in builds.values()},
    patches={p.name:sha(p) for p in c.glob('*.patch')},
    logs={p.name:sha(p) for p in (c/'logs').iterdir()})
assert not (c/'manifest.json').exists()
(c/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
print(json.dumps(m['binaries'],indent=2))

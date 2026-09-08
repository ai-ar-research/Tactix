#!/usr/bin/env python3
"""Freeze a tested append fast path without changing physical watch layout."""
import difflib
import hashlib
import json
from pathlib import Path
root=Path(__file__).resolve().parents[3]
c=root/'benchmarks/runs/20260907-5466-watch-pair'
source=root/'.claude/worktrees/runtime-watch-pair-5466'
parent=root/'.claude/worktrees/runtime-bound-advance-5459'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
fixture='watch-pair: 180 fixed cases, spare=732 boundary=708 digest=fce3246c046c23bf'
for arm in ('old','new'):
    assert (c/'logs'/f'fixture-{arm}.log').read_text().strip()==fixture+'\nexit=0'
builds=['build/pin-5466-watch-pair-release','build/pin-5467-watch-pair-bc']
for arm,b in zip(('release','bc'),builds):
    assert '100% tests passed out of 83' in (c/'logs'/f'ctest-{arm}.log').read_text()
    assert fixture in (root/b/'Testing/Temporary/LastTest.log').read_text()
assert sha(root/builds[0]/'tactix')==sha(root/builds[1]/'tactix')
files=['src/kernel/watch.hpp','tests/test_watch_pair.cpp']
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
    purpose='Avoid repeated one-word append checks and metadata writes when an existing literal slice has two spare words for a large watch.',
    motivation='Clean 5425 profile: kernel_rebuild_large_watches 2.45% self samples. Existing 5444 annotation identifies repeated inline push checks, size/holes and packed-layout work during its two-word append. This is an attribution lead, not a measured removable fraction.',
    references=['kissat/src/inlinevector.h::kissat_push_vectors and watch.h packed two-word large entry; preserve exact growth and usable-word lifecycle',
                'cadical/src/watch.hpp::Watch, whole-entry vector append'],
    contract='Fast path only when capacity-size>=2. Two stored words, size+2, holes-2; packed metadata is unchanged because binary count, order and size parity are unchanged. Otherwise retain the exact two-push growth/relocation path. No allocation, representation, public primitive, rule, guard, proof, scheduling or effort change.',
    fixture='180 fixed cases: nine lengths, five spare-capacity boundaries, tail/non-tail, binary-only/mixed layouts; repeated large and binary appends and compact/refill. Actual predecessor and candidate headers produce identical full physical-state digest fce3246c046c23bf; 732 spare and 708 boundary appends plus refills.',
    status='Isolated UNFUZZED candidate; cumulative parent 5459, excludes independent metadata 5426. Fuzz owner 5442 covers only 5438. No runtime gain or promotion claimed.',
    files={n:sha(source/n) for n in files},binaries={b+'/tactix':sha(root/b/'tactix') for b in builds},
    patches={p.name:sha(p) for p in c.glob('*.patch')},logs={p.name:sha(p) for p in (c/'logs').iterdir()})
assert not (c/'manifest.json').exists();(c/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
print(json.dumps(m['binaries'],indent=2))

#!/usr/bin/env python3
"""Freeze tested diagnostic-only deltas against both exact parent sources."""
import difflib
import hashlib
import json
from pathlib import Path
root=Path(__file__).resolve().parents[3]
c=root/'benchmarks/runs/20260908-5696-score-bump-observer'
assert '100% tests passed out of 89' in (c/'logs/ctest-tactix.log').read_text()
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
sources={}
for arm,before,after,changed in [
    ('tactix','.claude/worktrees/runtime-propagation-cursor-repair-5647','.claude/worktrees/runtime-score-bump-observer-5696',
     ['src/diagnostics/event_trace.cpp','tests/test_score_bump_observer.cpp']),
    ('kissat','tools/solvers/variants/kissat-imply-fast-5414','tools/solvers/variants/kissat-score-bump-observer-5696',['src/eventtrace.c'])]:
    parent,source=root/before,root/after
    names=sorted({str(p.relative_to(base)) for base in (parent,source) for folder in ('src','tests') for p in (base/folder).rglob('*') if p.is_file()})
    differences=[name for name in names if not (parent/name).exists() or not (source/name).exists() or sha(parent/name)!=sha(source/name)]
    assert sorted(differences)==sorted(changed),(arm,differences)
    patch=''
    for name in changed:
        a,b=parent/name,source/name
        patch+=''.join(difflib.unified_diff(a.read_text().splitlines(True) if a.exists() else [],b.read_text().splitlines(True),
            fromfile='a/'+name if a.exists() else '/dev/null',tofile='b/'+name))
    p=c/(arm+'.patch');assert not p.exists();p.write_text(patch)
    sources[arm]=dict(parent=before,source=after,files={n:sha(source/n) for n in names},patch_sha256=sha(p))
old=root/'benchmarks/runs/20260908-5649-propagation-cursor-validated/cumulative.patch'
(c/'cumulative.patch').write_text(old.read_text()+(c/'tactix.patch').read_text())
binaries=['build/pin-5696-score-bump-observer-trace/tactix','tools/solvers/variants/kissat-score-bump-observer-5696/build/kissat']
m=dict(purpose='Standalone selected analyzed-variable score-bump observer; no global active trace, algorithm or solver state change.',
    sources=sources,binaries={n:sha(root/n) for n in binaries},logs={p.name:sha(p) for p in (c/'logs').iterdir()},
    validation='89/89 fixed trace tests including three fork-isolated observer cases: exact selected rows and state invariance, missing path, missing selection. Both builds pass. Full per-arm parent replay/proof identity still required.',
    parent_candidate_authority='benchmarks/runs/20260908-5649-propagation-cursor-validated/manifest.json',
    parent_native_binary_sha256=sha(root/'tools/solvers/variants/kissat-imply-fast-5414/build/kissat'),
    test_consideration='New observer code requires new unit tests: inclusive range endpoints, event and positive/negative external selection, absent configuration, invalid payloads, exact output, sequence and unchanged score/increment/ticks/trail. Runtime parser tests and whole-parent replay supplement them.',
    limitations='Only existing analyzed-variable score_bump callbacks are observed. Initial/reorder/set/rescale operations are not individual events here; macro history5694 brackets them. Recorded increment is post-update current value; a rescale inside a bump can change its scale.',
    cumulative_patch_sha256=sha(c/'cumulative.patch'))
p=c/'manifest.json';assert not p.exists();p.write_text(json.dumps(m,indent=2)+'\n')
print('Frozen score observer:89/89 trace tests; no algorithm changes')

#!/usr/bin/env python3
"""Freeze the coupled entry repair and retain initial failure and terminal repair."""
import difflib
import hashlib
import json
from pathlib import Path
root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260907-5391-eliminate-dense-entry'
before = root / '.claude/worktrees/runtime-dense-publication-5387'
after = root / '.claude/worktrees/runtime-eliminate-dense-entry-5391'
files = ('src/policies/default_policy.cpp', 'tests/test_scheduling.cpp')
patch = ''.join(''.join(difflib.unified_diff((before / f).read_text().splitlines(True),
                                           (after / f).read_text().splitlines(True),
                                           fromfile='a/' + f, tofile='b/' + f)) for f in files)
(c / 'eliminate-entry.patch').write_text(patch)
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
m = dict(parent_source=str(before.relative_to(root)), source=str(after.relative_to(root)),
         mechanism='DefaultPolicy Eliminate uses existing kernel_flush_large_watches instead of deduplicate-only before connecting occurrences. Coupled predecessor 5387 suppresses disconnected sparse publication; existing phase exit reconnects.',
         reference='kissat/src/eliminate.c enter_dense_mode -> dense.c flush_large_watches; cadical/src/elim.cpp resets watches for elimination memory ownership.',
         validation=dict(trace_ctest='76/76, 20.79 seconds', release_ctest='77/77, 17.44 seconds', bc_ctest='77/77, 17.30 seconds',
                         fixture='Actual old connected/four large words versus repaired disconnected/zero; both restore four large/two binary words with no elimination. Actual native entry is disconnected.'),
         initial_failure='Initial scheduling assertion failed because a non-preprocess test slot was posed at zero conflicts and its observer was never called; standalone after arm exited 1, called=0. Initial logs retained.',
         claude_resolution='Bounded mandatory Claude review repaired fixture wiring only: one conflict, non-null meter, zero retry advice, admission/readout and non-vacuous restoration assertions. No src implementation edit, no solver defect, no fuzz required by this fixture repair. Terminal report retained.',
         status='Isolated UNFUZZED/unpromoted cumulative candidate. New runtime 5392 and B28+C5 5393 are separate pending gates.',
         binaries={b: sha(root / b / 'tactix') for b in ('build/pin-5391-eliminate-dense-entry', 'build/pin-5392-eliminate-dense-release', 'build/pin-5393-eliminate-dense-bc')},
         source_sha256={f: sha(after / f) for f in files},
         evidence_sha256={str(p.relative_to(c)): sha(p) for p in [c / 'eliminate-entry.patch', c / 'fixture.json', *sorted((c / 'logs').glob('*'))]})
p = c / 'manifest.json'
if p.exists(): assert json.loads(p.read_text()) == m
else: p.write_text(json.dumps(m, indent=2) + '\n')
print(c)

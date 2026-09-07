#!/usr/bin/env python3
"""Archive the exact isolated representation repair and its fixed validation."""
import difflib
import hashlib
import json
from pathlib import Path
root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260907-5387-dense-publication'
before = root / '.claude/worktrees/runtime-vivify-retry-5378'
after = root / '.claude/worktrees/runtime-dense-publication-5387'
files = ('src/kernel/kernel.cpp', 'tests/test_kernel.cpp')
patch = ''.join(''.join(difflib.unified_diff((before / f).read_text().splitlines(True),
                                           (after / f).read_text().splitlines(True),
                                           fromfile='a/' + f, tofile='b/' + f)) for f in files)
(c / 'dense-publication.patch').write_text(patch)
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
m = dict(parent_source=str(before.relative_to(root)), source=str(after.relative_to(root)),
         mechanism='New large clauses publish sparse watches only while large_watches_connected. Dense occurrences and all existing proof/advice publication remain unchanged.',
         reference='kissat/src/clause.c new_large_clause selects watch_reference versus connect_clause; cadical/src/clause.cpp new_factor_clause asserts !watching and publishes occurrences.',
         validation=dict(trace_ctest='76/76, 21.25 seconds', release_ctest='77/77, 20.88 seconds',
                         fixture='Actual native dense-only, old Tactix four unwanted sparse words, repaired zero; dense occurrences preserved. Old exit 1 is expected representation mismatch, not solver failure.'),
         compile_issue='Initial native fixture lacked inlinevector.h and failed to link. Header corrected before native execution; no solver failure.',
         status='Isolated UNFUZZED/unpromoted cumulative candidate; 5388 runtime and 5389 B28+C5 in flight.',
         binaries={b: sha(root / b / 'tactix') for b in ('build/pin-5387-dense-publication', 'build/pin-5388-dense-publication-release', 'build/pin-5389-dense-publication-bc')},
         source_sha256={f: sha(after / f) for f in files},
         evidence_sha256={str(p.relative_to(c)): sha(p) for p in [c / 'dense-publication.patch', c / 'fixture.json', *sorted((c / 'logs').glob('*'))]})
p = c / 'manifest.json'
if p.exists(): assert json.loads(p.read_text()) == m
else: p.write_text(json.dumps(m, indent=2) + '\n')
print(c)

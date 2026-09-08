#!/usr/bin/env python3
"""Retain the initial fixture expectation and its original full-suite failures."""
from pathlib import Path
import hashlib
import json
import shutil
root=Path(__file__).resolve().parents[3]
c=root/'benchmarks/runs/20260908-5718-root-conflict-reuse'
source=root/'.claude/worktrees/runtime-root-conflict-reuse-5718/tests/test_root_conflict_reuse.cpp'
target=c/'initial-fixture.cpp'
assert not target.exists()
shutil.copy2(source,target)
(c/'initial-fixture-expectation.json').write_text(json.dumps(dict(
    source_sha256=hashlib.sha256(target.read_bytes()).hexdigest(),
    diagnosis='Fixture assumed canonical registry learning used DefaultPolicy chronological retention. Source conflictanalysis_apply calls backjump_apply with jump_level (zero here); only reuse owns its own chrono target. First failing case:size2/depth103/mode0/early roots/reuse disabled/canonical/DRAT. This is a test expectation mismatch,not a solver correctness oracle failure.',
    fix='Expected retained level is depth-1 only for actual reuse above the chrono threshold; otherwise registry target is zero. Solver source remains unchanged.'),indent=2)+'\n')

#!/usr/bin/env python3
"""Reuse fixed prefix, Rank 1–40 and B28+C5 contracts for the budget repair."""
from pathlib import Path

source=Path(__file__).with_name('prepare-inprocess-gates-5433.py').read_text()
changes={
 '20260907-5430-inprocess-root':'20260907-5450-eliminate-limit',
 'build/pin-5430-inprocess-root-release':'build/pin-5450-eliminate-limit-release',
 'build/pin-5432-inprocess-root-bc':'build/pin-5452-eliminate-limit-bc',
 'build/pin-5431-inprocess-root-trace':'build/pin-5451-eliminate-limit-trace',
 '20260907-5424-rank35-rephase-prefix':'20260907-5436-rank37-inprocess-prefix',
 '20260907-5433-rank35-inprocess-prefix':'20260907-5453-rank37-eliminate-limit-prefix',
 "'tactix-5424-'":"'tactix-5436-'",
 "'tactix-5433-'":"'tactix-5453-'",
 '.claude/worktrees/runtime-inprocess-root-5430':'.claude/worktrees/runtime-eliminate-limit-5450',
 'Restore Probe/Eliminate root entry and compare the complete Rank-35 prefix at 100000 conflicts against the Rephase-only witness and native reference.':
   'Enforce the shared BVE ceiling before optional forward rounds. Test closure of the common Rank-37 boundary at conflict 29761 and extension beyond the former 7504348 matching decision rows; compare all rows through 100000 conflicts.',
 '20260907-5434-inprocess-root-ranks1-40':'20260907-5454-eliminate-limit-ranks1-40',
 'Full unchanged Rank 1–40 comparison for Probe/Eliminate root closure on the Rephase stack; metadata ablation 5426 is not included.':
   'Full unchanged Rank 1–40 comparison for cumulative phase-resolution-limit repair on replacement cursor 5445; independent metadata 5426 excluded.',
 'Close a missing general root-fixpoint and tick-accounting boundary before simplifier rule admission. Measure all work/runtime/memory outcomes without selecting favorable trajectories.':
   'Close the missing general phase-resolution stop. Measure all work/runtime/memory outcomes; changed trajectories are expected, and there is no exact-work premise or favorable-rank selection.',
 'Old/new actual scheduler fixtures; trace 77/77 and independent matching Release builds 78/78 each; UNFUZZED.':
   '12 actual old/new phase scenarios; trace 80/80 and independent matching Release builds 81/81 each; UNFUZZED.',
 '20260907-5435-inprocess-root-BC-gate':'20260907-5455-eliminate-limit-BC-gate',
 '5435-inprocess-root-BC':'5455-eliminate-limit-BC',
 'Unchanged B28+C5 gate for the cumulative Probe/Eliminate root-closure candidate against production.':
   'Unchanged B28+C5 gate for the cumulative phase-resolution-limit candidate against production.',
 'General phase-entry restoration preserves all verdicts and avoids unacceptable cumulative regression.':
   'General phase-budget boundary restoration preserves all verdicts and avoids unacceptable cumulative regression.',
 'Independent immutable Release 5430/5432, identical hashes, 78/78 tests each; trace/conflict-stop/shadow/journal disabled.':
   'Independent immutable Release 5450/5452, identical hashes, 81/81 tests each; trace/conflict-stop/shadow/journal disabled.',
}
for old,new in changes.items():
    assert old in source,old
    source=source.replace(old,new)
exec(compile(source,__file__,'exec'))

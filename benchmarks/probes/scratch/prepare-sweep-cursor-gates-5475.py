#!/usr/bin/env python3
"""Retain the exact-work three-pair and fixed B28+C5 campaign contracts."""
from pathlib import Path
source = Path(__file__).with_name('prepare-watch-pair-gates-5468.py').read_text()
for old, new in [
    ('20260907-5466-watch-pair', '20260908-5473-sweep-cursor'),
    ('build/pin-5466-watch-pair-release', 'build/pin-5473-sweep-cursor-release'),
    ('build/pin-5467-watch-pair-bc', 'build/pin-5474-sweep-cursor-bc'),
    ('20260907-5468-watch-pair-rank35-runtime', '20260908-5475-sweep-cursor-rank35-runtime'),
    ('build/pin-5459-bound-advance-release/tactix', 'build/pin-5466-watch-pair-release/tactix'),
    ('20260907-5459-bound-advance/cumulative.patch', '20260907-5466-watch-pair/cumulative.patch'),
    ('180 actual old/new whole-watch-state cases; independent identical Release builds each pass 83/83 fixed tests; UNFUZZED.',
     '588 actual old/new Sweep-ledger cases; independent identical Release builds each pass 84/84 fixed tests; UNFUZZED.'),
    ('A two-word append within existing capacity removes repeated checks and metadata writes while preserving complete work and proof bytes.',
     'Private wraparound membership cursors avoid repeated retained-prefix scans in Sweep bookkeeping while preserving complete work and proof bytes.'),
    ('against exact bound-lifecycle predecessor 5459', 'against exact watch-pair predecessor 5466'),
    ('20260907-5469-watch-pair-BC-gate', '20260908-5476-sweep-cursor-BC-gate'),
    ('5469-watch-pair-BC', '5476-sweep-cursor-BC'),
    ('Unchanged cumulative B28+C5 gate for exact-layout two-word watch append against production.',
     'Unchanged cumulative B28+C5 gate for private Sweep membership cursors against production.'),
    ('Preserve expected verdicts and cumulative regression behavior with the exact-layout append optimization.',
     'Preserve expected verdicts and cumulative regression behavior with exact Sweep-ledger membership optimization.'),
    ('Independent immutable Release 5466/5467, matching SHA-256, 83/83 tests each.',
     'Independent immutable Release 5473/5474, matching SHA-256, 84/84 tests each.'),
]:
    assert old in source, old
    source = source.replace(old, new)
exec(compile(source, __file__, 'exec'))

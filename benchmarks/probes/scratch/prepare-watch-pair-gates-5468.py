#!/usr/bin/env python3
"""Reuse exact-work three-pair timing and fixed B28+C5 regression contracts."""
from pathlib import Path
source=Path(__file__).with_name('prepare-vivify-max-gates-5440.py').read_text()
changes={
 '20260907-5438-vivify-max':'20260907-5466-watch-pair',
 'build/pin-5438-vivify-max-release':'build/pin-5466-watch-pair-release',
 'build/pin-5439-vivify-max-bc':'build/pin-5467-watch-pair-bc',
 '20260907-5440-vivify-max-rank35-runtime':'20260907-5468-watch-pair-rank35-runtime',
 'build/pin-5430-inprocess-root-release/tactix':'build/pin-5459-bound-advance-release/tactix',
 '20260907-5430-inprocess-root/cumulative.patch':'20260907-5459-bound-advance/cumulative.patch',
 '144 exact old/new schedule/pose cases; independent identical Release builds each pass 79/79 fixed tests; UNFUZZED.':
   '180 actual old/new whole-watch-state cases; independent identical Release builds each pass 83/83 fixed tests; UNFUZZED.',
 'Linear maximum selection avoids unnecessary irredundant literal sorting while preserving exact work and proof bytes on the clean profile witness.':
   'A two-word append within existing capacity removes repeated checks and metadata writes while preserving complete work and proof bytes.',
 'against phase-entry 5430':'against exact bound-lifecycle predecessor 5459',
 '20260907-5441-vivify-max-BC-gate':'20260907-5469-watch-pair-BC-gate',
 '5441-vivify-max-BC':'5469-watch-pair-BC',
 'Unchanged cumulative B28+C5 gate for linear maximum-count Vivify ranking against production.':
   'Unchanged cumulative B28+C5 gate for exact-layout two-word watch append against production.',
 'Preserve expected verdicts and cumulative regression behavior with the exact-rank scheduling optimization.':
   'Preserve expected verdicts and cumulative regression behavior with the exact-layout append optimization.',
 'Independent immutable Release 5438/5439, matching SHA-256, 79/79 tests each.':
   'Independent immutable Release 5466/5467, matching SHA-256, 83/83 tests each.',
}
for old,new in changes.items():
    assert source.count(old)==1,old;source=source.replace(old,new)
exec(compile(source,__file__,'exec'))

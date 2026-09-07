#!/usr/bin/env python3
"""Observe the same memory ownership after completing the dense entry transition."""
from pathlib import Path
root = Path(__file__).resolve().parents[3]
source = (root / 'benchmarks/probes/scratch/prepare-memory-ownership-5386.py').read_text()
source = source.replace('20260907-5379-vivify-retry-runtime', '20260907-5392-eliminate-entry-runtime')
source = source.replace('20260907-5386-memory-ownership', '20260907-5394-memory-dense-entry')
source = source.replace('build/pin-5378-vivify-retry', 'build/pin-5391-eliminate-dense-entry')
source = source.replace('The 4.7-GB Release Rank-21 peak versus 1.4-GB Kissat warrants ownership attribution: distinguish dense/index backing, semantic formula/search state and allocator retention.',
                        'Test whether the complete Eliminate dense entry removes the giant BVE sparse-watch arena observed in 5386 and unchanged by publication-only 5390; retain residual ownership for the next general repair.')
source = source.replace('existing event-trace/conflict-stop-enabled 5378 pin', 'existing event-trace/conflict-stop-enabled 5391 pin')
exec(compile(source, __file__, 'exec'), {'__file__': __file__})

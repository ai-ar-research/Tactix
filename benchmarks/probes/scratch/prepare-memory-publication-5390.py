#!/usr/bin/env python3
"""Repeat the existing ownership diagnostic with the isolated publication repair."""
from pathlib import Path
root = Path(__file__).resolve().parents[3]
source = (root / 'benchmarks/probes/scratch/prepare-memory-ownership-5386.py').read_text()
source = source.replace('20260907-5379-vivify-retry-runtime', '20260907-5388-dense-publication-runtime')
source = source.replace('20260907-5386-memory-ownership', '20260907-5390-memory-publication')
source = source.replace('build/pin-5378-vivify-retry', 'build/pin-5387-dense-publication')
source = source.replace('The 4.7-GB Release Rank-21 peak versus 1.4-GB Kissat warrants ownership attribution: distinguish dense/index backing, semantic formula/search state and allocator retention.',
                        'Test whether suppressing sparse-watch publication in physical dense mode removes the giant BVE watch-arena growth observed in 5386; compare ownership and checked logical work.')
source = source.replace('existing event-trace/conflict-stop-enabled 5378 pin', 'existing event-trace/conflict-stop-enabled 5387 pin')
exec(compile(source, __file__, 'exec'), {'__file__': __file__})

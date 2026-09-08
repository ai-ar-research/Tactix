#!/usr/bin/env python3
"""Reuse the unchanged exact-work timing and B28+C5 protocols."""
from pathlib import Path

source = Path(__file__).with_name('prepare-vivify-max-gates-5440.py').read_text()
changes = {
    '20260907-5438-vivify-max': '20260907-5445-replacement-cursor',
    'build/pin-5438-vivify-max-release': 'build/pin-5445-replacement-cursor-release',
    'build/pin-5439-vivify-max-bc': 'build/pin-5446-replacement-cursor-bc',
    '20260907-5440-vivify-max-rank35-runtime': '20260907-5447-replacement-cursor-rank35-runtime',
    'build/pin-5430-inprocess-root-release/tactix': 'build/pin-5438-vivify-max-release/tactix',
    '20260907-5430-inprocess-root/cumulative.patch': '20260907-5438-vivify-max/cumulative.patch',
    '144 exact old/new schedule/pose cases; independent identical Release builds each pass 79/79 fixed tests; UNFUZZED.':
        '840 exact old/new replacement-state cases; independent identical Release builds each pass 80/80 fixed tests; UNFUZZED.',
    'Linear maximum selection avoids unnecessary irredundant literal sorting while preserving exact work and proof bytes on the clean profile witness.':
        'Wraparound membership cursors avoid repeated unchanged-prefix scans during large replacement while preserving exact work and proof bytes.',
    'against phase-entry 5430': 'against exact predecessor 5438',
    '20260907-5441-vivify-max-BC-gate': '20260907-5448-replacement-cursor-BC-gate',
    '5441-vivify-max-BC': '5448-replacement-cursor-BC',
    'Unchanged cumulative B28+C5 gate for linear maximum-count Vivify ranking against production.':
        'Unchanged cumulative B28+C5 gate for replacement membership cursors against production.',
    'Preserve expected verdicts and cumulative regression behavior with the exact-rank scheduling optimization.':
        'Preserve expected verdicts and cumulative regression behavior with exact wraparound membership queries.',
    'Independent immutable Release 5438/5439, matching SHA-256, 79/79 tests each.':
        'Independent immutable Release 5445/5446, matching SHA-256, 80/80 tests each.',
}
for old, new in changes.items():
    assert source.count(old) == 1, old
    source = source.replace(old, new)
exec(compile(source, __file__, 'exec'))

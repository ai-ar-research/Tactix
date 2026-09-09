#!/usr/bin/env python3
"""Freeze the existing B28+C5 composition with independent cache pins."""
from pathlib import Path
source = Path(__file__).with_name('prepare-substitute-retry-gates-6236.py')
code = source.read_text()
marker = "\nold = root/'benchmarks/runs/20260909-6183-rank32-current-prefix'"
assert code.count(marker) == 1
code = code.split(marker)[0]
for old, new in {
    '20260909-6231-substitute-retry-build': '20260909-6383-vivify-count-cache-build',
    '20260909-6234-substitute-retry-references': '20260909-6386-vivify-count-cache-proofs',
    '20260909-6236-substitute-retry-BC-gate': '20260909-6387-vivify-count-cache-BC-gate',
    'build/pin-6231-substitute-retry-': 'build/pin-6383-vivify-count-cache-',
    'canonical Substitute removal publication': 'private Vivify leading-count cache',
    'Reference-backed retry repair may change search work. No exact parent work or speedup assumed.': 'Cache preserves every scheduling comparison and should preserve complete parent6357 proof/non-timing work. This broad candidate/production gate detects solve loss/regression; separate predecessor paired timings price the cache.',
}.items():
    assert old in code, old
    code = code.replace(old, new)
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})

#!/usr/bin/env python3
"""Build only the new independent diagnostic sources, then run all trace tests."""
from pathlib import Path
source = Path(__file__).with_name('build-bve-full-heap-5957.py')
code = source.read_text()
for old, new in {
    '20260908-5956-bve-full-heap-controls': '20260908-6004-clause-pose-series-controls',
    'runtime-bve-full-heap-5956': 'runtime-clause-pose-series-6004',
    'pin-5956-bve-full-heap-trace': 'pin-6004-clause-pose-series-trace',
    'kissat-bve-full-heap-5956': 'kissat-clause-pose-series-6004',
}.items():
    assert old in code
    code = code.replace(old, new)
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})

#!/usr/bin/env python3
"""Build new learning observers and run the complete trace suite."""
from pathlib import Path
source = Path(__file__).with_name('build-bve-full-heap-5957.py')
code = source.read_text()
for old, new in {
    '20260908-5956-bve-full-heap-controls': '20260908-6030-learned-stage-controls',
    'runtime-bve-full-heap-5956': 'runtime-learned-stage-6030',
    'pin-5956-bve-full-heap-trace': 'pin-6030-learned-stage-trace',
    'kissat-bve-full-heap-5956': 'kissat-learned-stage-6030',
}.items():
    assert old in code; code = code.replace(old, new)
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})

#!/usr/bin/env python3
"""Build independent candidate-level observers and run the full trace suite."""
from pathlib import Path
source = Path(__file__).with_name('build-bve-full-heap-5957.py')
code = source.read_text()
for old, new in {
    '20260908-5956-bve-full-heap-controls': '20260908-6016-vivify-pose-controls',
    'runtime-bve-full-heap-5956': 'runtime-vivify-pose-6016',
    'pin-5956-bve-full-heap-trace': 'pin-6016-vivify-pose-trace',
    'kissat-bve-full-heap-5956': 'kissat-vivify-pose-6016',
}.items():
    assert old in code; code = code.replace(old, new)
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})

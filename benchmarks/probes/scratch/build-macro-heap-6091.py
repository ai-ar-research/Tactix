#!/usr/bin/env python3
"""Build isolated macro heap observers and run existing full trace tests."""
from pathlib import Path
source = Path(__file__).with_name('build-bve-full-heap-5957.py')
code = source.read_text()
for old,new in {
    '20260908-5956-bve-full-heap-controls':'20260908-6090-macro-heap-controls',
    'runtime-bve-full-heap-5956':'runtime-macro-heap-6090',
    'pin-5956-bve-full-heap-trace':'pin-6090-macro-heap-trace',
    'kissat-bve-full-heap-5956':'kissat-macro-heap-6090',
}.items():
    assert old in code; code=code.replace(old,new)
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})

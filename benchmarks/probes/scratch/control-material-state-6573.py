#!/usr/bin/env python3
"""Run the unchanged shared-touch fixture against the new state-only objects."""
from pathlib import Path
source = Path(__file__).with_name('check-touch-consumers-6568.py')
code = source.read_text()
for old, new in {
    '20260909-6568-touch-consumer-contract': '20260909-6573-material-shared-touch-control',
    '20260909-6424-activation-stamp-build': '20260909-6571-material-state-build',
    'build/pin-6424-activation-stamp-release': 'build/pin-6571-material-state-release',
}.items():
    assert old in code, old
    code = code.replace(old, new)
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})

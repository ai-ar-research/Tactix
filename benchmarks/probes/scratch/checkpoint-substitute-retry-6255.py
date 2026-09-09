#!/usr/bin/env python3
"""New immutable checkpoint; do not overwrite the earlier partial snapshots."""
from pathlib import Path
source = Path(__file__).with_name('checkpoint-substitute-retry-6247.py')
code = source.read_text()
old = '2026-09-09-substitute-retry-checkpoint-6247.json'
assert code.count(old) == 1
exec(compile(code.replace(old, '2026-09-09-substitute-retry-checkpoint-6255.json'), str(source), 'exec'),
     {'__file__': str(source), '__name__': '__main__'})

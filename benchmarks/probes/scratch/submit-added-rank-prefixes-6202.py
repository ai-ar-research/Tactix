#!/usr/bin/env python3
"""Submit only the two independently pinned added-rank prefix campaigns."""
from pathlib import Path
source = Path(__file__).with_name('submit-rank32-extension-6184.py')
for name in ('20260909-6200-rank50-current-prefix', '20260909-6201-rank49-current-prefix'):
    code = source.read_text()
    old = '20260909-6183-rank32-current-prefix'
    assert code.count(old) == 1
    code = code.replace(old, name)
    exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})

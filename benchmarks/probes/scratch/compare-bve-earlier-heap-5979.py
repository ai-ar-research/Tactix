#!/usr/bin/env python3
"""Replay and reconstruct the earlier selected pop, keeping5958 frozen."""
from pathlib import Path
source = Path(__file__).with_name('compare-bve-full-heap-5960.py')
code = source.read_text()
for old, new in {'20260908-5958-bve-full-heap': '20260908-5978-bve-earlier-heap',
                 '122629': '462198', '225614': '60705', '4465': '8930',
                 '4472': '8954', '165965': '465378'}.items():
    assert old in code
    code = code.replace(old, new)
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})

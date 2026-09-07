#!/usr/bin/env python3
"""Use the existing final checked/clean full-view oracle unchanged."""
from pathlib import Path
source = Path(__file__).with_name('seal-vivify-ranks-5383.py').read_text()
before = '20260907-5383-vivify-retry-ranks1-40'
assert source.count(before) == 1
source = source.replace(before, '20260907-5395-eliminate-entry-ranks1-40')
source = source.replace("'5383 pending'", "'5395 pending'")
exec(compile(source, __file__, 'exec'))

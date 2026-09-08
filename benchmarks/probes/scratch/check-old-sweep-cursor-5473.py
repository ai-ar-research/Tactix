#!/usr/bin/env python3
"""Run the identical new fixed fixture against frozen predecessor objects."""
from pathlib import Path
source = Path(__file__).with_name('check-old-replacement-cursor-5445.py').read_text()
for old, new in [
    ('pin-5438-vivify-max-release', 'pin-5466-watch-pair-release'),
    ('replacement-cursor-old-5445', 'sweep-cursor-old-5473'),
    ('runtime-replacement-cursor-5445', 'runtime-sweep-cursor-5473'),
    ('test_replacement_cursor.cpp', 'test_sweep_cursor.cpp'),
    ('runtime-vivify-max-5438', 'runtime-watch-pair-5466'),
    ('20260907-5445-replacement-cursor', '20260908-5473-sweep-cursor'),
]:
    assert old in source
    source = source.replace(old, new)
exec(compile(source, __file__, 'exec'))

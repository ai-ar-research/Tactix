#!/usr/bin/env python3
"""Independent full Release suites for the isolated Sweep membership repair."""
from pathlib import Path
source = Path(__file__).with_name('build-replacement-cursor-5445.py').read_text()
for old, new in [
    ('runtime-replacement-cursor-5445', 'runtime-sweep-cursor-5473'),
    ('20260907-5445-replacement-cursor', '20260908-5473-sweep-cursor'),
    ('replacement-cursor-{arm}', 'sweep-cursor-{arm}'),
    ("('release', 5445), ('bc', 5446)", "('release', 5473), ('bc', 5474)"),
]:
    assert old in source
    source = source.replace(old, new)
exec(compile(source, __file__, 'exec'))

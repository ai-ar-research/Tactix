#!/usr/bin/env python3
"""Independent Release builds and full fixed suites for retained eligibility."""
from pathlib import Path
source = Path(__file__).with_name('build-replacement-cursor-5445.py').read_text()
for old, new in [
    ('runtime-replacement-cursor-5445', 'runtime-sweep-retained-5510'),
    ('20260907-5445-replacement-cursor', '20260908-5510-sweep-retained'),
    ('replacement-cursor-{arm}', 'sweep-retained-{arm}'),
    ("('release', 5445), ('bc', 5446)", "('release', 5510), ('bc', 5512)"),
]:
    assert old in source
    source = source.replace(old, new)
exec(compile(source, __file__, 'exec'))

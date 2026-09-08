#!/usr/bin/env python3
"""Independent full Release tests for the watch-pair fast path."""
from pathlib import Path
source=Path(__file__).with_name('build-replacement-cursor-5445.py').read_text()
for old,new in [('runtime-replacement-cursor-5445','runtime-watch-pair-5466'),
                ('20260907-5445-replacement-cursor','20260907-5466-watch-pair'),
                ('replacement-cursor-{arm}','watch-pair-{arm}'),
                ("('release', 5445), ('bc', 5446)","('release', 5466), ('bc', 5467)")]:
    assert old in source;source=source.replace(old,new)
exec(compile(source,__file__,'exec'))

#!/usr/bin/env python3
"""Apply the unchanged checked/clean B28+C5 oracle to the Sweep candidate."""
from pathlib import Path
source = Path(__file__).with_name('seal-replacement-cursor-gate-5448.py').read_text()
source = source.replace('20260907-5448-replacement-cursor-BC-gate',
                        '20260908-5476-sweep-cursor-BC-gate')
exec(compile(source, __file__, 'exec'))

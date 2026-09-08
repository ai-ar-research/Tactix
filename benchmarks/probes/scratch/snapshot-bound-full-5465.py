#!/usr/bin/env python3
"""Score all forty bound-candidate pairs without blocking on proof checks."""
from pathlib import Path
source = Path(__file__).with_name('snapshot-rephase-full-5423.py').read_text()
source = source.replace('20260907-5423-rephase-root-ranks1-40',
                        '20260907-5465-bound-advance-ranks1-40')
source = source.replace('5423 solver records pending', '5465 solver records pending')
exec(compile(source, __file__, 'exec'))

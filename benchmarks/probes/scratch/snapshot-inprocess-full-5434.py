#!/usr/bin/env python3
"""Score all forty phase-entry pairs without waiting for certificate checking."""
from pathlib import Path
source = Path(__file__).with_name('snapshot-rephase-full-5423.py').read_text()
source = source.replace('20260907-5423-rephase-root-ranks1-40', '20260907-5434-inprocess-root-ranks1-40')
source = source.replace('5423 solver records pending', '5434 solver records pending')
exec(compile(source, __file__, 'exec'))

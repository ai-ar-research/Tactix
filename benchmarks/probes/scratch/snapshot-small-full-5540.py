#!/usr/bin/env python3
"""Freeze the complete40-pair solver view, never a partial aggregate."""
from pathlib import Path
source = Path(__file__).with_name('snapshot-rephase-full-5423.py').read_text()
source = source.replace('20260907-5423-rephase-root-ranks1-40', '20260908-5535-sweep-small-ranks1-40')
source = source.replace('5423 solver records pending', '5535 solver records pending')
exec(compile(source, __file__, 'exec'))

#!/usr/bin/env python3
"""Score the complete phase-budget solver view without waiting for checks."""
from pathlib import Path
source=Path(__file__).with_name('snapshot-rephase-full-5423.py').read_text()
source=source.replace('20260907-5423-rephase-root-ranks1-40','20260907-5454-eliminate-limit-ranks1-40').replace('5423 solver records pending','5454 solver records pending')
exec(compile(source,__file__,'exec'))

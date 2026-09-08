#!/usr/bin/env python3
"""Apply the complete stopped-stream oracle to the fixed Rank-37 boundary."""
from pathlib import Path
source=Path(__file__).with_name('compare-rank37-prefix-5436.py').read_text()
source=source.replace('20260907-5436-rank37-inprocess-prefix','20260907-5453-rank37-eliminate-limit-prefix').replace('5436 pending','5453 pending')
exec(compile(source,__file__,'exec'))

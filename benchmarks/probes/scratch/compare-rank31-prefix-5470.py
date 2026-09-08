#!/usr/bin/env python3
"""Apply complete-stream and controlled-stop validation to Rank 31."""
from pathlib import Path
source=Path(__file__).with_name('compare-rank37-prefix-5436.py').read_text()
source=source.replace('20260907-5436-rank37-inprocess-prefix','20260907-5470-rank31-bound-prefix').replace('5436 pending','5470 pending')
exec(compile(source,__file__,'exec'))

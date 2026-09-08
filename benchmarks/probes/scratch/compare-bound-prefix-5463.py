#!/usr/bin/env python3
"""Reuse the complete-stream, pin and cleanup oracle for the bound repair."""
from pathlib import Path
source=Path(__file__).with_name('compare-rank37-prefix-5436.py').read_text()
source=source.replace('20260907-5436-rank37-inprocess-prefix','20260907-5463-rank37-bound-prefix').replace('5436 pending','5463 pending')
exec(compile(source,__file__,'exec'))

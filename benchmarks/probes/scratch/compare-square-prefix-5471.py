#!/usr/bin/env python3
"""Reuse full decision-stream/controlled-stop validation on short square."""
from pathlib import Path
source=Path(__file__).with_name('compare-rank37-prefix-5436.py').read_text()
source=source.replace('20260907-5436-rank37-inprocess-prefix','20260907-5471-square-bound-prefix').replace('5436 pending','5471 pending')
exec(compile(source,__file__,'exec'))

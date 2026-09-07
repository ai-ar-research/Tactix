#!/usr/bin/env python3
"""Retain the same endpoint/heap oracle for the smaller lifecycle bracket."""
from pathlib import Path
source = Path(__file__).with_name('compare-heap-bracket-5404.py').read_text()
source = source.replace('20260907-5404-rank35-heap-bracket', '20260907-5405-rank35-heap-lifecycle')
source = source.replace("!= 12:", "!= 6:").replace('5404 pending', '5405 pending').replace('clean_stops=12', 'clean_stops=6')
exec(compile(source, __file__, 'exec'))

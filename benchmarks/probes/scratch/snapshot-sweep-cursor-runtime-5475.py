#!/usr/bin/env python3
"""Require complete exact-work/proof pairs before interpreting Sweep timing."""
from pathlib import Path
source = Path(__file__).with_name('snapshot-watch-runtime-5428.py').read_text()
source = source.replace('20260907-5428-watch-truncate-rank35-runtime',
                        '20260908-5475-sweep-cursor-rank35-runtime')
source = source.replace('5428 solver results pending', '5475 solver results pending')
exec(compile(source, __file__, 'exec'))

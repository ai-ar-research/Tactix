#!/usr/bin/env python3
"""Close the complete checked/clean Rank 1--40 gate, never a partial view."""
from pathlib import Path
source = Path(__file__).with_name('seal-vivify-ranks-5383.py').read_text()
source = source.replace('20260907-5383-vivify-retry-ranks1-40',
                        '20260908-5485-sweep-cursor-ranks1-40')
source = source.replace('5383 pending', '5485 pending')
exec(compile(source, __file__, 'exec'))

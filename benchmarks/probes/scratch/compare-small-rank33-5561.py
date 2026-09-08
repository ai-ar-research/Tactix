#!/usr/bin/env python3
"""Use the complete-stream and stopped-proof oracle on the new Rank33 witness."""
from pathlib import Path
source = Path(__file__).with_name('compare-rank37-prefix-5436.py').read_text()
source = source.replace('20260907-5436-rank37-inprocess-prefix', '20260908-5558-rank33-small-prefix')
source = source.replace('5436 pending', '5558 pending')
exec(compile(source, __file__, 'exec'))

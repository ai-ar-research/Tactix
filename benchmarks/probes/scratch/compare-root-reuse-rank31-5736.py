#!/usr/bin/env python3
"""Compare the complete current Rank31 decision prefix without historical assumptions."""
from pathlib import Path
source = Path(__file__).with_name('compare-rank37-prefix-5436.py').read_text()
source = source.replace('20260907-5436-rank37-inprocess-prefix',
                        '20260908-5735-root-reuse-rank31-prefix')
source = source.replace('5436 pending', '5735 pending')
exec(compile(source, __file__, 'exec'))

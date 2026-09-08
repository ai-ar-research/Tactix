#!/usr/bin/env python3
"""Apply complete-stream and clean-stop validation to the Rank-32 witness."""
from pathlib import Path
source = Path(__file__).with_name('compare-rank37-prefix-5436.py').read_text()
source = source.replace('20260907-5436-rank37-inprocess-prefix',
                        '20260908-5479-rank32-bound-prefix')
source = source.replace('5436 pending', '5479 pending')
exec(compile(source, __file__, 'exec'))

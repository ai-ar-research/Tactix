#!/usr/bin/env python3
"""Locate the boundary preceding Rank 37's validated later decision split."""
from pathlib import Path
source = Path(__file__).with_name('localize-bound-boundary-5463.py').read_text()
source = source.replace('20260907-5463-rank37-bound-prefix',
                        '20260908-5478-rank37-bound-extended')
source = source.replace("summary['matching_rows'] >= 7918162",
                        "summary['stream_from'] == 100000 and summary['matching_rows'] == 25001084")
exec(compile(source, __file__, 'exec'))

#!/usr/bin/env python3
"""Reuse portable boundary checks on the newly matched Rank-31 prefix."""
from pathlib import Path
source = Path(__file__).with_name('localize-bound-boundary-5463.py').read_text()
source = source.replace('20260907-5463-rank37-bound-prefix',
                        '20260907-5470-rank31-bound-prefix')
source = source.replace("summary['matching_rows'] >= 7918162",
                        "summary['matching_rows'] == 51491857")
exec(compile(source, __file__, 'exec'))

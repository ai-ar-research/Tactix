#!/usr/bin/env python3
"""Localize the first portable Rank-37 rule boundary after complete traces."""
from pathlib import Path
source = Path(__file__).with_name('localize-rephase-boundary-5424.py').read_text()
source = source.replace('20260907-5424-rank35-rephase-prefix', '20260907-5436-rank37-inprocess-prefix')
source = source.replace("summary['prior_13833_prefix_preserved']", "summary['matching_rows'] == 7504348")
exec(compile(source, __file__, 'exec'))

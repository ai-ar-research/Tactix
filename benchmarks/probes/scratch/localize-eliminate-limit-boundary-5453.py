#!/usr/bin/env python3
"""Locate the next Rank-37 normalized boundary after the phase-limit repair."""
from pathlib import Path
source=Path(__file__).with_name('localize-rephase-boundary-5424.py').read_text()
source=source.replace('20260907-5424-rank35-rephase-prefix','20260907-5453-rank37-eliminate-limit-prefix')
source=source.replace("summary['prior_13833_prefix_preserved']","summary['matching_rows'] == 7918162")
exec(compile(source,__file__,'exec'))

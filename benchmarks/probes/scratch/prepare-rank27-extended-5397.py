#!/usr/bin/env python3
"""Extend the exact existing three-arm Rank-27 diagnostic, without new code."""
from pathlib import Path
source = Path(__file__).with_name('prepare-rank27-prefix-5396.py').read_text()
source = source.replace('20260907-5396-rank27-prefix', '20260907-5397-rank27-extended')
source = source.replace('stop_conflict=25000', 'stop_conflict=100000')
source = source.replace('Locate first decision/boundary divergence and whether dense entry advances or removes it before selecting a repair.',
                        '5396 matches all 7,154,988 decision rows through 25,000 conflicts; extend to 100,000 to locate the first worse boundary before selecting a repair.')
exec(compile(source, __file__, 'exec'))

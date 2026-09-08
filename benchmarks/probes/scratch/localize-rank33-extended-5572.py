#!/usr/bin/env python3
"""Locate portable boundaries before the validated Rank33 late decision split."""
from pathlib import Path
base = Path(__file__).with_name('localize-rephase-boundary-5424.py').read_text()
base = base.replace('20260907-5424-rank35-rephase-prefix', '20260908-5567-rank33-small-extended')
base = base.replace("summary['prior_13833_prefix_preserved']",
                    "summary['prior_common_prefix_replayed'] and summary['matching_rows'] == 18013384")
exec(compile(base, __file__, 'exec'))
nonbinary = base.replace('    if t != k:',
    "    if t is None or k is None or {n:v for n,v in t.items() if n != 'binary'} != {n:v for n,v in k.items() if n != 'binary'}:")
nonbinary = nonbinary.replace('first_coarse_boundary=first', 'first_nonbinary_boundary=first')
nonbinary = nonbinary.replace('boundary-localization.json', 'nonbinary-localization.json')
nonbinary = nonbinary.replace('Active and clause counts locate a boundary,',
    'Earlier binary-only differences are unresolved. Active and clause counts locate a boundary,')
exec(compile(nonbinary, __file__, 'exec'))

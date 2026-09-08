#!/usr/bin/env python3
"""Record the next clause/active boundary without dismissing binary differences."""
from pathlib import Path
source = Path(__file__).with_name('localize-rephase-boundary-5424.py').read_text()
source = source.replace('20260907-5424-rank35-rephase-prefix',
                        '20260908-5478-rank37-bound-extended')
source = source.replace("summary['prior_13833_prefix_preserved']",
                        "summary['stream_from'] == 100000 and summary['matching_rows'] == 25001084")
source = source.replace('    if t != k:', "    if t is None or k is None or {n:v for n,v in t.items() if n != 'binary'} != {n:v for n,v in k.items() if n != 'binary'}:")
source = source.replace('first_coarse_boundary=first', 'first_nonbinary_boundary=first')
source = source.replace('boundary-localization.json', 'nonbinary-localization.json')
source = source.replace('Active and clause counts locate a boundary,',
    'Earlier binary-only differences remain unresolved, not dismissed as neutral. Active and clause counts locate a boundary,')
exec(compile(source, __file__, 'exec'))

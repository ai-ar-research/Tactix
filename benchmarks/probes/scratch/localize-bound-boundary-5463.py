#!/usr/bin/env python3
"""Locate the next portable boundary after validating the bound-repair stream."""
from pathlib import Path
source = Path(__file__).with_name('localize-rephase-boundary-5424.py').read_text()
source = source.replace('20260907-5424-rank35-rephase-prefix',
                        '20260907-5463-rank37-bound-prefix')
source = source.replace("summary['prior_13833_prefix_preserved']",
                        "summary['matching_rows'] >= 7918162")
source = source.replace('assert first is not None',
                        '# A repaired prefix may have no portable boundary difference.')
source = source.replace("rows[max(0, first['index']-4):first['index']+1]",
                        "(rows[max(0, first['index']-4):first['index']+1] if first else [])")
source = source.replace('result = dict(first_coarse_boundary=first,',
                        'result = dict(portable_boundaries_identical=first is None, '
                        'boundary_counts={a: len(r) for a, r in boundaries.items()}, '
                        'first_coarse_boundary=first,')
exec(compile(source, __file__, 'exec'))

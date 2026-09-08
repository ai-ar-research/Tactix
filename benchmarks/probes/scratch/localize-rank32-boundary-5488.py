#!/usr/bin/env python3
"""Locate portable boundaries around Rank 32's validated first decision split."""
from pathlib import Path
source = Path(__file__).with_name('localize-bound-boundary-5463.py').read_text()
source = source.replace('20260907-5463-rank37-bound-prefix',
                        '20260908-5479-rank32-bound-prefix')
source = source.replace("summary['matching_rows'] >= 7918162",
                        "summary['matching_rows'] == 18583703")
exec(compile(source, __file__, 'exec'))

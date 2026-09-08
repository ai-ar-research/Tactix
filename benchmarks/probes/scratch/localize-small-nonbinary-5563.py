#!/usr/bin/env python3
"""Separate binary-only counts from the first nonbinary portable difference."""
from pathlib import Path
source = Path(__file__).with_name('localize-rank37-nonbinary-5491.py').read_text()
source = source.replace('20260908-5478-rank37-bound-extended', '20260908-5537-rank32-small-prefix')
source = source.replace("summary['stream_from'] == 100000 and summary['matching_rows'] == 25001084",
                        "summary['matching_rows'] == 18696140")
exec(compile(source, __file__, 'exec'))

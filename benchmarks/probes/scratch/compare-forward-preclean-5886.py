#!/usr/bin/env python3
"""Compare the full repaired Rank31 stream with its parent and cached native."""
from pathlib import Path
source = Path(__file__).with_name('compare-forward-unit-rank31-5814.py')
code = source.read_text().replace('20260908-5810-forward-unit-rank31-prefix',
                                 '20260908-5879-forward-preclean-rank31-prefix')
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})

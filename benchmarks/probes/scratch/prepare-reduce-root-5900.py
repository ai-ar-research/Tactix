#!/usr/bin/env python3
"""Independent Reduce-guidance repair from5849,without regressing5870 preclean."""
from pathlib import Path
source = Path(__file__).with_name('prepare-forward-preclean-5870.py')
code = source.read_text().replace('runtime-forward-preclean-5870', 'runtime-reduce-root-5900').replace('20260908-5870-forward-preclean', '20260908-5900-reduce-root')
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})

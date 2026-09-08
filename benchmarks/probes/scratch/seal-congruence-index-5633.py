#!/usr/bin/env python3
"""Unchanged strict complete B28+C5 checker and cleanup gate."""
from pathlib import Path
source=Path(__file__).with_name('seal-vivify-census-5553.py').read_text()
source=source.replace('20260908-5550-vivify-census-BC-gate','20260908-5628-congruence-index-BC-gate')
exec(compile(source,__file__,'exec'))

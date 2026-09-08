#!/usr/bin/env python3
"""Validate the complete post100K suffix and unchanged shared-prefix boundary replay."""
from pathlib import Path
source=Path(__file__).with_name('compare-rank33-extended-5568.py').read_text()
source=source.replace('20260908-5567-rank33-small-extended','20260908-5634-congruence-index-extended')
source=source.replace('5567 pending','5634 pending')
exec(compile(source,__file__,'exec'))

#!/usr/bin/env python3
"""Locate remaining portable differences after validated root-satisfied cleanup."""
from pathlib import Path
source = Path(__file__).with_name('localize-rank33-extended-5572.py').read_text()
source = source.replace('20260908-5567-rank33-small-extended',
                        '20260908-5634-congruence-index-extended')
exec(compile(source, __file__, 'exec'))

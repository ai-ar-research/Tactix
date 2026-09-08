#!/usr/bin/env python3
"""Retain the unchanged66-record cumulative regression oracle."""
from pathlib import Path
source = Path(__file__).with_name('seal-congruence-gates-5372.py').read_text()
old = "[('20260907-5371-congruence-prefix-runtime',12),('20260907-5372-congruence-prefix-BC-gate',66)]"
assert source.count(old) == 1
source = source.replace(old, "[('20260908-5550-vivify-census-BC-gate',66)]")
exec(compile(source, __file__, 'exec'))

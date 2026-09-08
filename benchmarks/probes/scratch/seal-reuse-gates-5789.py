#!/usr/bin/env python3
"""Seal root/public-target B28+C5 with the unchanged strict66-record oracle."""
from pathlib import Path
source = Path(__file__).with_name('seal-congruence-gates-5372.py').read_text()
before = "[('20260907-5371-congruence-prefix-runtime',12),('20260907-5372-congruence-prefix-BC-gate',66)]"
after = "[('20260908-5727-root-reuse-BC-gate',66),('20260908-5754-reuse-target-BC-gate',66)]"
assert source.count(before) == 1
exec(compile(source.replace(before, after), __file__, 'exec'))

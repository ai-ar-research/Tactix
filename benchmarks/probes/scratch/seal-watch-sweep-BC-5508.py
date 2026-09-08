#!/usr/bin/env python3
"""Apply the unchanged fixed-membership checked/clean B28+C5 oracle."""
from pathlib import Path
source = Path(__file__).with_name('seal-congruence-gates-5372.py').read_text()
before = "[('20260907-5371-congruence-prefix-runtime',12),('20260907-5372-congruence-prefix-BC-gate',66)]"
assert source.count(before) == 1
source = source.replace(before, "[('20260907-5469-watch-pair-BC-gate',66),('20260908-5476-sweep-cursor-BC-gate',66)]")
exec(compile(source, __file__, 'exec'))

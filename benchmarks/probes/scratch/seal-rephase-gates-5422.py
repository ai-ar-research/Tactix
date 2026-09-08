#!/usr/bin/env python3
"""Apply the existing final checked/clean oracles to the unchanged gates."""
from pathlib import Path

directory = Path(__file__).resolve().parent
source = (directory / 'seal-congruence-gates-5372.py').read_text()
before = "[('20260907-5371-congruence-prefix-runtime',12),('20260907-5372-congruence-prefix-BC-gate',66)]"
assert source.count(before) == 1
source = source.replace(before, "[('20260907-5422-rephase-root-BC-gate',66)]")
exec(compile(source, __file__, 'exec'))
source = (directory / 'seal-vivify-ranks-5383.py').read_text()
source = source.replace('20260907-5383-vivify-retry-ranks1-40', '20260907-5423-rephase-root-ranks1-40')
source = source.replace("'5383 pending'", "'5423 pending'")
exec(compile(source, __file__, 'exec'))

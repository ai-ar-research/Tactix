#!/usr/bin/env python3
"""Apply unchanged final checked/clean full-view and B28+C5 oracles."""
from pathlib import Path
d=Path(__file__).resolve().parent
source=(d/'seal-congruence-gates-5372.py').read_text()
before="[('20260907-5371-congruence-prefix-runtime',12),('20260907-5372-congruence-prefix-BC-gate',66)]"
assert source.count(before)==1
source=source.replace(before,"[('20260907-5455-eliminate-limit-BC-gate',66)]")
exec(compile(source,__file__,'exec'))
source=(d/'seal-vivify-ranks-5383.py').read_text().replace('20260907-5383-vivify-retry-ranks1-40','20260907-5454-eliminate-limit-ranks1-40').replace("'5383 pending'","'5454 pending'")
exec(compile(source,__file__,'exec'))

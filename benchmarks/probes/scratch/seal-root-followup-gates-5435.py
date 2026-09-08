#!/usr/bin/env python3
"""Reuse unchanged final checked/clean full-view and B28+C5 oracles."""
from pathlib import Path
directory = Path(__file__).resolve().parent
for number,name in [(5429,'watch-truncate'),(5435,'inprocess-root')]:
    source = (directory/'seal-congruence-gates-5372.py').read_text()
    before = "[('20260907-5371-congruence-prefix-runtime',12),('20260907-5372-congruence-prefix-BC-gate',66)]"
    assert source.count(before) == 1
    source = source.replace(before,f"[('20260907-{number}-{name}-BC-gate',66)]")
    exec(compile(source,__file__,'exec'))
source = (directory/'seal-vivify-ranks-5383.py').read_text().replace('20260907-5383-vivify-retry-ranks1-40','20260907-5434-inprocess-root-ranks1-40').replace("'5383 pending'","'5434 pending'")
exec(compile(source,__file__,'exec'))

#!/usr/bin/env python3
"""Compare the longer prefix, with post-startup common boundary normalization."""
from pathlib import Path
source = Path(__file__).with_name('compare-rank27-prefix-5396.py').read_text()
source = source.replace('20260907-5396-rank27-prefix', '20260907-5397-rank27-extended')
source = source.replace("'5396 pending'", "'5397 pending'")
source = source.replace("if arm == 'kissat' and r[2] not in names:",
                        "if int(r[4]) == 0 or (arm == 'kissat' and r[2] not in names) or (arm != 'kissat' and r[2] not in names.values()):")
exec(compile(source, __file__, 'exec'))

#!/usr/bin/env python3
"""Keep the existing checked-gate contract for the coupled entry candidate."""
from pathlib import Path
source = Path(__file__).with_name('seal-congruence-gates-5372.py').read_text()
for before, after in [('20260907-5371-congruence-prefix-runtime', '20260907-5392-eliminate-entry-runtime'),
                      ('20260907-5372-congruence-prefix-BC-gate', '20260907-5393-eliminate-entry-BC-gate')]:
    assert source.count(before) == 1
    source = source.replace(before, after)
exec(compile(source, __file__, 'exec'))

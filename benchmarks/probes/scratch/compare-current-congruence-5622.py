#!/usr/bin/env python3
"""Supply the runner origin to the frozen5612 comparator's isolated namespace."""
from pathlib import Path
source = Path(__file__).with_name('compare-current-congruence-5612.py').read_text()
anchor = "exec(compile(source, __file__, 'exec'), {})"
assert source.count(anchor) == 1
source = source.replace(anchor, "exec(compile(source, __file__, 'exec'), {'__file__': __file__})")
exec(compile(source, __file__, 'exec'))

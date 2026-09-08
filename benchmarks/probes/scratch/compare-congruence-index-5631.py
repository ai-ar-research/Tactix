#!/usr/bin/env python3
"""Compare the repaired full100K stream and actual Congruence clause outputs."""
from pathlib import Path
source=Path(__file__).with_name('compare-current-congruence-5612.py').read_text()
source=source.replace('20260908-5611-current-congruence-output','20260908-5627-congruence-index-rank33-prefix')
source=source.replace('5611 pending','5627 pending')
source=source.replace("exec(compile(source, __file__, 'exec'), {})", "exec(compile(source, __file__, 'exec'), {'__file__': __file__})")
exec(compile(source,__file__,'exec'))

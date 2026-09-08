#!/usr/bin/env python3
"""Compare the full new decision stream with contiguous fixed native traces."""
from pathlib import Path
source = Path(__file__).with_name('compare-forward-admission-rank32-5839.py')
code = source.read_text().replace('20260908-5837-forward-admission-rank32-prefix', '20260908-5969-bve-active-pop-rank32-prefix')
code = code.replace("row['conflicts'] == '179463'", "row['conflicts'] == '271079'")
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})

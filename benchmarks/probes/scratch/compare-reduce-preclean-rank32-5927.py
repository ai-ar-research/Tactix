#!/usr/bin/env python3
"""Compare complete combined-candidate decisions with both cached native streams."""
from pathlib import Path
source = Path(__file__).with_name('compare-forward-admission-rank32-5839.py')
code = source.read_text().replace('20260908-5837-forward-admission-rank32-prefix',
                                '20260908-5925-reduce-preclean-rank32-prefix')
exec(compile(code,str(source),'exec'), {'__file__':str(source),'__name__':'__main__'})

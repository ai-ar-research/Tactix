#!/usr/bin/env python3
"""Seal complete composition BC screen separately from pending proof checks."""
from pathlib import Path
source=Path(__file__).with_name('seal-preclean-bc-5887.py')
code=source.read_text().replace('20260908-5878-forward-preclean-BC-gate','20260908-5924-reduce-preclean-BC-gate')
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})

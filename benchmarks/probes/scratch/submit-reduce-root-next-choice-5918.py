#!/usr/bin/env python3
"""Submit the new current5900 next-choice diagnostic only."""
from pathlib import Path
source=Path(__file__).with_name('submit-rank32-choice-5884.py')
code=source.read_text().replace('20260908-5883-rank32-choice-frontier','20260908-5917-reduce-root-next-choice')
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})

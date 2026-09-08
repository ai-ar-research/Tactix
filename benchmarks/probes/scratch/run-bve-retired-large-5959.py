#!/usr/bin/env python3
"""Fixed large-antecedent controls against the retained immutable source."""
from pathlib import Path
source=Path(__file__).with_name('run-bve-retired-reentry-5952.py')
code=source.read_text().replace('20260908-5952-bve-retired-reentry-controls','20260908-5959-bve-retired-large-controls/repaired').replace('probe-bve-retired-reentry-5952.cpp','probe-bve-retired-large-5959.cpp')
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})

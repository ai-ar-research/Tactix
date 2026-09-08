#!/usr/bin/env python3
"""Submit corrected diagnostic only; cancelled5945 remains immutable."""
from pathlib import Path
source=Path(__file__).with_name('submit-bve-heap-frontier-5946.py')
code=source.read_text().replace('20260908-5945-bve-heap-frontier','20260908-5949-bve-heap-lifecycle')
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})

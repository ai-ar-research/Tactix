#!/usr/bin/env python3
"""New immutable campaign after cancelling the incomplete observer contract."""
from pathlib import Path
source=Path(__file__).with_name('prepare-bve-heap-frontier-5945.py')
code=source.read_text().replace('20260908-5945-bve-heap-frontier','20260908-5949-bve-heap-lifecycle').replace('5945-bve-heap-frontier','5949-bve-heap-lifecycle').replace('tactix-5945-','tactix-5949-')
code=code.replace('kissat-bve-heap-observer-5944','kissat-bve-heap-lifecycle-5948').replace('20260908-5944-bve-heap-observer-controls/repaired','20260908-5948-bve-heap-lifecycle-controls')
code=code.replace('eight bad selector or missing-initial controls','seven bad selector controls and actual construction/per-round reset regression')
code=code.replace('native changes only read-only selected heap journal plus post-push callback','native changes only read-only selected heap journal plus schedule-begin/post-push callbacks')
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})

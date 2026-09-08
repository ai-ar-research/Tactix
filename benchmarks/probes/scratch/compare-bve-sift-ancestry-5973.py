#!/usr/bin/env python3
"""Reuse the strict observer parser/replay checks with the new declared subjects."""
from pathlib import Path
source = Path(__file__).with_name('compare-bve-heap-lifecycle-5951.py')
code = source.read_text().replace('20260908-5949-bve-heap-lifecycle', '20260908-5972-bve-sift-ancestry')
old = "SUBJECTS={'225614','225641','225620','89339'}"
assert code.count(old) == 1
code = code.replace(old, "SUBJECTS={'225614','127295','129082','40787','107019','41090','60705','89339'}")
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})

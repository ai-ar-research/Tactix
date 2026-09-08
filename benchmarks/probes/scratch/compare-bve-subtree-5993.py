#!/usr/bin/env python3
"""Retarget strict selected-state replay without altering earlier evidence."""
from pathlib import Path
source=Path(__file__).with_name('compare-bve-heap-lifecycle-5951.py')
code=source.read_text().replace('20260908-5949-bve-heap-lifecycle','20260908-5992-bve-subtree-ancestry')
code=code.replace("SUBJECTS={'225614','225641','225620','89339'}",
                  "SUBJECTS={'225614','60705','477747','244396','5409','3343','3531','242204'}")
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})

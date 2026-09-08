#!/usr/bin/env python3
"""Close only fully checked and residue-clean cumulative regression records."""
from pathlib import Path
source = Path(__file__).with_name('seal-factor-support-5602.py').read_text()
source = source.replace('20260908-5598-factor-support-BC-gate', '20260908-5651-propagation-cursor-BC-gate')
exec(compile(source, __file__, 'exec'))

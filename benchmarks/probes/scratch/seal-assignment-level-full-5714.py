#!/usr/bin/env python3
"""Close the full gate only with80 checked and cleaned records."""
from pathlib import Path
source=Path(__file__).with_name('seal-factor-support-full-5609.py').read_text()
source=source.replace('20260908-5604-factor-support-ranks1-40','20260908-5712-assignment-level-ranks1-40')
source=source.replace('5604 pending','5712 pending')
exec(compile(source,__file__,'exec'))

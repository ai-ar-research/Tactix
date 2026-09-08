#!/usr/bin/env python3
"""Seal only all80 checked and cleaned full-view records."""
from pathlib import Path
source = Path(__file__).with_name('seal-small-full-5540.py').read_text()
source = source.replace('20260908-5535-sweep-small-ranks1-40', '20260908-5604-factor-support-ranks1-40')
source = source.replace('5535 pending', '5604 pending')
exec(compile(source, __file__, 'exec'))

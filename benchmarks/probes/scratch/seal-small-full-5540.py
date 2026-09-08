#!/usr/bin/env python3
"""Close only all80 checked/clean records in the fixed full view."""
from pathlib import Path
source = Path(__file__).with_name('seal-vivify-ranks-5383.py').read_text()
source = source.replace('20260907-5383-vivify-retry-ranks1-40', '20260908-5535-sweep-small-ranks1-40')
source = source.replace('5383 pending', '5535 pending')
exec(compile(source, __file__, 'exec'))

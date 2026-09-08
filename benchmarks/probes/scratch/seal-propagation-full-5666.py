#!/usr/bin/env python3
"""Close only the complete 80-record checked and cleaned full view."""
from pathlib import Path
source = Path(__file__).with_name('seal-factor-support-full-5609.py').read_text()
source = source.replace('20260908-5604-factor-support-ranks1-40', '20260908-5664-propagation-cursor-ranks1-40')
source = source.replace('5604 pending', '5664 pending')
exec(compile(source, __file__, 'exec'))

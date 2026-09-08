#!/usr/bin/env python3
"""Freeze the complete 40-pair solver view; pending checks remain provisional."""
from pathlib import Path
source = Path(__file__).with_name('snapshot-factor-support-full-5608.py').read_text()
source = source.replace('20260908-5604-factor-support-ranks1-40', '20260908-5664-propagation-cursor-ranks1-40')
source = source.replace('5604 solver records pending', '5664 solver records pending')
exec(compile(source, __file__, 'exec'))

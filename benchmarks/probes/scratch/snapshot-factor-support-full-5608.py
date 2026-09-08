#!/usr/bin/env python3
"""Freeze only the complete40-pair solver score, preserving all pending-check labels."""
from pathlib import Path
source = Path(__file__).with_name('snapshot-small-full-5540.py').read_text()
source = source.replace('20260908-5535-sweep-small-ranks1-40', '20260908-5604-factor-support-ranks1-40')
source = source.replace('5535 solver records pending', '5604 solver records pending')
exec(compile(source, __file__, 'exec'))

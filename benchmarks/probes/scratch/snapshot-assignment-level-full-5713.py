#!/usr/bin/env python3
"""Freeze all40 assignment-level solver pairs before asynchronous checks finish."""
from pathlib import Path
source=Path(__file__).with_name('snapshot-factor-support-full-5608.py').read_text()
source=source.replace('20260908-5604-factor-support-ranks1-40','20260908-5712-assignment-level-ranks1-40')
source=source.replace('5604 solver records pending','5712 solver records pending')
exec(compile(source,__file__,'exec'))

#!/usr/bin/env python3
"""Enforce exact complete statistics/proof bytes before interpreting timing."""
from pathlib import Path
source=Path(__file__).with_name('snapshot-watch-runtime-5428.py').read_text()
source=source.replace('20260907-5428-watch-truncate-rank35-runtime','20260907-5468-watch-pair-rank35-runtime').replace('5428 solver results pending','5468 solver results pending')
exec(compile(source,__file__,'exec'))

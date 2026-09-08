#!/usr/bin/env python3
"""Reuse the exact-work/proof-identity three-pair timing oracle."""
from pathlib import Path
source = Path(__file__).with_name('snapshot-watch-runtime-5428.py').read_text()
source = source.replace('20260907-5428-watch-truncate-rank35-runtime', '20260907-5447-replacement-cursor-rank35-runtime')
source = source.replace('5428 solver results pending', '5447 solver results pending')
exec(compile(source, __file__, 'exec'))

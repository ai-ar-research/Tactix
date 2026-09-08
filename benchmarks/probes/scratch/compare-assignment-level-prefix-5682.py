#!/usr/bin/env python3
"""Validate unchanged complete per-arm million-conflict traces and proof bytes."""
from pathlib import Path
source = Path(__file__).with_name('compare-propagation-prefix-5654.py').read_text()
source = source.replace('20260908-5650-propagation-cursor-rank33-prefix','20260908-5677-assignment-level-rank33-prefix')
source = source.replace('5650 pending','5677 pending')
exec(compile(source,__file__,'exec'))

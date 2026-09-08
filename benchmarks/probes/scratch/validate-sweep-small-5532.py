#!/usr/bin/env python3
"""Preserve earlier fixture failures; validate the canonical dense boundary."""
from pathlib import Path
source = Path(__file__).with_name('validate-sweep-small-5527.py').read_text()
source = source.replace('final', 'verified').replace('5527', '5532')
exec(compile(source, __file__, 'exec'))

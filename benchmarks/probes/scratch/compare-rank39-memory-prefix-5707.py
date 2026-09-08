#!/usr/bin/env python3
"""Preserve failed5703; validate the complete producer schema from pinned code."""
from pathlib import Path
source=Path(__file__).with_name('compare-rank39-memory-prefix-5703.py').read_text()
source=source.replace('validate-rank39-memory-5688.py','memory-row-parser-5706.py')
source=source.replace('parser_fixtures=11','parser_fixtures=24')
exec(compile(source,__file__,'exec'))

#!/usr/bin/env python3
"""One changed-source fuzz owner; expose only the terminal CLI response."""
from pathlib import Path
source=Path(__file__).with_name('run-claude-periodic-fuzz-5442.py').read_text()
source=source.replace('claude-periodic-fuzz-5442','claude-periodic-fuzz-5765')
exec(compile(source,__file__,'exec'))

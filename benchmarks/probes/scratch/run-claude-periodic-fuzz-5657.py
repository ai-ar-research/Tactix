#!/usr/bin/env python3
"""Start one changed-source fuzz checkpoint; only terminal output is exposed."""
from pathlib import Path
source = Path(__file__).with_name('run-claude-periodic-fuzz-5442.py').read_text()
source = source.replace('claude-periodic-fuzz-5442', 'claude-periodic-fuzz-5657')
exec(compile(source, __file__, 'exec'))

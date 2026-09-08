#!/usr/bin/env python3
"""Start exactly one changed-source checkpoint; expose only its terminal owner report."""
from pathlib import Path
source = Path(__file__).with_name('run-claude-periodic-fuzz-5442.py').read_text()
source = source.replace('claude-periodic-fuzz-5442', 'claude-periodic-fuzz-5610')
exec(compile(source, __file__, 'exec'))

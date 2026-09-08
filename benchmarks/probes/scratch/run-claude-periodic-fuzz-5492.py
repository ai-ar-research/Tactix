#!/usr/bin/env python3
"""Launch the latest-source owner; expose only its completed terminal report."""
from pathlib import Path
source = Path(__file__).with_name('run-claude-periodic-fuzz-5442.py').read_text()
source = source.replace('claude-periodic-fuzz-5442', 'claude-periodic-fuzz-5492')
exec(compile(source, __file__, 'exec'))

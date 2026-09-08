#!/usr/bin/env python3
"""Launch the bounded unit-crash owner and expose only the terminal response."""
from pathlib import Path
source = Path(__file__).with_name('run-claude-periodic-fuzz-5442.py').read_text()
source = source.replace('claude-periodic-fuzz-5442', 'claude-congruence-index-repair-5623')
source = source.replace('Claude fuzz owner launched', 'Claude bounded repair owner launched')
exec(compile(source, __file__, 'exec'))

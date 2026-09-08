#!/usr/bin/env python3
"""Launch bounded crash repair, exposing only its terminal response."""
from pathlib import Path
source = Path(__file__).with_name('run-claude-periodic-fuzz-5442.py').read_text()
source = source.replace('claude-periodic-fuzz-5442', 'claude-propagation-cursor-repair-5647')
source = source.replace('Claude fuzz owner launched', 'Claude bounded repair owner launched')
exec(compile(source, __file__, 'exec'))

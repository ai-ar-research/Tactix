#!/usr/bin/env python3
"""Check the coupled-entry memory trace against its checked Release arm."""
from pathlib import Path
source = Path(__file__).with_name('seal-memory-ownership-5386.py').read_text()
source = source.replace('20260907-5386-memory-ownership', '20260907-5394-memory-dense-entry')
source = source.replace("'5386", "'5394")
exec(compile(source, __file__, 'exec'))

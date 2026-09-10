#!/usr/bin/env python3
"""Instrument the complete BVE successor, including XOR boundary cases."""
from pathlib import Path
source = Path(__file__).with_name('sanitize-bve-import-7114.py')
code = source.read_text().replace('7114-bve-import-sanitizers', '7118-bve-import-bounds-sanitizers')
code = code.replace('7110-bve-import', '7116-bve-import-bounds')
code = code.replace('full BVE and BVE-memory fixed tests;', 'full BVE (including XOR arity/model boundaries) and BVE-memory fixed tests;')
exec(compile(code, str(source), 'exec'), {'__file__': str(Path(__file__)), '__name__': '__main__'})

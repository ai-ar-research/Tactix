#!/usr/bin/env python3
"""Validate separate7299 exact-Release process-specific RSS diagnostic."""
from pathlib import Path
base = Path(__file__).with_name('validate-proc-memory-7298.py')
code = base.read_text().replace('7298', '7299')
exec(compile(code, str(base), 'exec'), {'__file__': str(Path(__file__)), '__name__': '__main__'})

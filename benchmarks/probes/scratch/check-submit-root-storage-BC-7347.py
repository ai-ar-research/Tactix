#!/usr/bin/env python3
"""Same strict reader and full-width submit for corrected preparation7347."""
from pathlib import Path
base = Path(__file__).with_name('check-submit-root-storage-BC-7346.py')
code = base.read_text().replace('20260911-7346-root-storage-BC','20260911-7347-root-storage-BC')
exec(compile(code, str(base), 'exec'), {'__file__':str(Path(__file__)), '__name__':'__main__'})

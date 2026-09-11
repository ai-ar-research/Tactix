#!/usr/bin/env python3
"""Default read-only all-field comparison; one-shot optional --submit."""
from pathlib import Path
base=Path(__file__).with_name('check-submit-root-storage-BC-7346.py')
code=base.read_text().replace('20260911-7346-root-storage-BC','20260911-7370-lucky-roots-BC')
exec(compile(code,str(base),'exec'),dict(__file__=str(Path(__file__)),__name__='__main__'))

#!/usr/bin/env python3
"""Repeat the original144 fixtures after repairing the real reattach failure."""
from pathlib import Path
base=Path(__file__).with_name('check-lucky-root-proofs-7368.py')
code=base.read_text()
for before,after in {
    '20260911-7369-lucky-roots-validation':'20260911-7373-lucky-reattach-build',
    '20260911-7368-lucky-root-proofs':'20260911-7374-lucky-proofs',
    '7366-lucky-roots':'7373-lucky-reattach',
    "for p in (Path(__file__),driver,authority,":"for p in (Path(__file__),base,driver,authority,",
}.items():
    assert before in code,before;code=code.replace(before,after)
exec(compile(code,str(base),'exec'),dict(__file__=str(Path(__file__)),__name__='__main__',base=base))

#!/usr/bin/env python3
"""Prepare the combined Lucky/reattachment successor under the same BC gate."""
from pathlib import Path
base=Path(__file__).with_name('prepare-lucky-roots-BC-7370.py')
code=base.read_text()
for before,after in {
    '7370-lucky-roots-BC':'7375-lucky-BC',
    '20260911-7369-lucky-roots-validation':'20260911-7373-lucky-reattach-build',
    '20260911-7368-lucky-root-proofs':'20260911-7374-lucky-proofs',
    '7366-lucky-roots':'7373-lucky-reattach',
    'check-submit-lucky-roots-BC-7370.py':'check-submit-lucky-BC-7375.py',
    "for p in (Path(__file__),":"for p in (Path(__file__),base,",
    'Release124 twice/Trace123;16 focused sanitizer fixtures twice':'Release125 twice/Trace124;17 focused sanitizer fixtures twice; inherited reattachment duplicate-map defect independently reproduced and repaired',
}.items():
    assert before in code,before;code=code.replace(before,after)
exec(compile(code,str(base),'exec'),dict(__file__=str(Path(__file__)),__name__='__main__',base=base))

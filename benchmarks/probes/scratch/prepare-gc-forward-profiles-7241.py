#!/usr/bin/env python3
"""Use reviewed7233 machine calls for complete GC/Reduce profiles."""
from pathlib import Path
base = Path(__file__).with_name('prepare-root-binary-profiles-7230.py')
code = base.read_text()
for before,after in {
    '7230-root-binary':'7241-gc-forward',
    '7226-root-binary':'7240-gc-forward',
    '7221-root-binary':'7238-gc-forward',
    '7218-root-binary':'7233-gc-forward',
    'fresh7218/7162':'fresh7233/7162',
    'Fresh7218/7162':'Fresh7233/7162',
    'for7218 versus':'for7233 versus',
    'Unprofiled7226':'Unprofiled7240',
    'unchanged7226':'unchanged7240',
    'tactix-root-binary-whole-lifecycle-three-wave-profiles-v1':'tactix-gc-forward-whole-lifecycle-three-wave-profiles-v1',
    '0x5078b3':'0x507cf3',
    '0x508163':'0x5085a3',
    '0x508d02':'0x509142',
}.items():
    assert before in code,before
    code = code.replace(before,after)
exec(compile(code,str(base),'exec'), {'__file__':str(Path(__file__)), '__name__':'__main__'})

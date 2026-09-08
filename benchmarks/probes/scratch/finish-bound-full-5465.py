#!/usr/bin/env python3
"""Complete the unsubmitted full-view scaffold after skipping runtime pycache."""
import hashlib
from pathlib import Path
root=Path(__file__).resolve().parents[3]
c=root/'benchmarks/runs/20260907-5465-bound-advance-ranks1-40'
old=root/'benchmarks/runs/20260907-5454-eliminate-limit-ranks1-40'
assert not (c/'manifest.json').exists()
assert set(p.name for p in c.iterdir())=={'pins','logs','results','provisional','claims'}
for name in ('logs','results','provisional','claims'):assert not list((c/name).iterdir())
for p in (c/'pins').iterdir():
    assert p.is_file() and hashlib.sha256(p.read_bytes()).digest()==hashlib.sha256((old/'pins'/p.name).read_bytes()).digest()
source=Path(__file__).with_name('prepare-bound-gates-5462.py').read_text()
prefix=source[:source.index("old=runs/'20260907-5455-eliminate-limit-BC-gate'")]
loop=source[source.index('for kind,oldname,newname in ['):]
loop=loop.replace("    ('bc','20260907-5455-eliminate-limit-BC-gate','20260907-5464-bound-advance-BC-gate'),\n",'')
loop=loop.replace("if p.is_file():shutil.copy2(p,c/'pins'/p.name)",
                  "if p.is_file() and not (c/'pins'/p.name).exists():shutil.copy2(p,c/'pins'/p.name)")
prefix=prefix.replace('    c.mkdir(exist_ok=False)','    assert c.name=="20260907-5465-bound-advance-ranks1-40"\n    c.mkdir(exist_ok=True)')
prefix=prefix.replace('(c/d).mkdir()','(c/d).mkdir(exist_ok=True)')
exec(compile(prefix+loop,__file__,'exec'))

#!/usr/bin/env python3
"""Account for configure's exact build-path relocation without changing evidence."""
from pathlib import Path
source=Path(__file__).with_name('freeze-score-bump-observer-5697.py').read_text()
anchor='    assert sorted(differences)==sorted(changed),(arm,differences)'
assert source.count(anchor)==1
source=source.replace(anchor,"""    if arm=='kissat':
        name='src/makefile'
        assert (parent/name).read_text().replace(str(parent),str(source))==(source/name).read_text()
        assert name in differences
        differences.remove(name)
"""+anchor)
source=source.replace("p=c/(arm+'.patch');assert not p.exists();p.write_text(patch)",
    "p=c/(arm+'.patch')\n    if p.exists():assert p.read_text()==patch\n    else:p.write_text(patch)")
source=source.replace("m=dict(purpose=", "m=dict(freeze_recovery='5697 identified configure-generated src/makefile path relocation.5700 requires exact parent-to-child absolute-path substitution,keeps its hash,and verifies existing Tactix patch without replacing it.',purpose=")
exec(compile(source,__file__,'exec'))

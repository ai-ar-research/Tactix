#!/usr/bin/env python3
"""Resume the report audit after fixing pre-delivery LaTeX line overflow."""
from pathlib import Path
source = Path(__file__).with_name('freeze-choice-ownership-5691.py').read_text()
anchor = "exec(compile(source,__file__,'exec'))"
assert source.count(anchor) == 1
source = source.replace(anchor, '''source = source.replace(
    "assert not target.exists();shutil.copy2(main,target)",
    "if target.exists(): assert sha(target)==sha(main)\\nelse: shutil.copy2(main,target)")
source = source.replace("result=dict(fixed_authorities={},observed_at=",
    "result=dict(audit_recovery='5691 stopped on report Overfull assertion before freezing audit. Pre-delivery TeX line wrapping fixed and compiled twice; existing CTest copy hash must equal current source log, never overwritten.',fixed_authorities={},observed_at=")
exec(compile(source,__file__,'exec'))''')
exec(compile(source,__file__,'exec'))

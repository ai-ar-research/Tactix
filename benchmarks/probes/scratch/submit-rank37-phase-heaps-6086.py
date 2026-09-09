#!/usr/bin/env python3
"""Submit two separate paired diagnostic snapshots with immutable pins."""
from pathlib import Path
source=Path(__file__).with_name('submit-root-lock-rank37-6072.py')
for name in ('20260908-6084-rank37-prephase-heap','20260908-6085-rank37-postphase-heap'):
    code=source.read_text().replace('20260908-6070-root-lock-rank37-prefix',name)
    code=code.replace('Cached native reused.','Native rerun for missing before/after phase heap states only;fixed timing unchanged.')
    exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})

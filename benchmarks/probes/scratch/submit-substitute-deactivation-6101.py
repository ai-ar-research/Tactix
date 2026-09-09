#!/usr/bin/env python3
"""Submit isolated deactivation sequence pair without changing other campaigns."""
from pathlib import Path
source=Path(__file__).with_name('submit-root-lock-rank37-6072.py')
code=source.read_text().replace('20260908-6070-root-lock-rank37-prefix','20260908-6100-rank37-substitute-deactivation')
code=code.replace('Cached native reused.','Native rerun only for missing deactivation order;fixed timing unchanged.')
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})

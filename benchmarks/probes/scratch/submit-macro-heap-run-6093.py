#!/usr/bin/env python3
"""Submit only the isolated missing-fields macro-heap pair."""
from pathlib import Path
source=Path(__file__).with_name('submit-root-lock-rank37-6072.py')
code=source.read_text().replace('20260908-6070-root-lock-rank37-prefix','20260908-6092-rank37-macro-heaps')
code=code.replace('Cached native reused.','Native rerun only for missing complete macro heap transitions;timing reference unchanged.')
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})

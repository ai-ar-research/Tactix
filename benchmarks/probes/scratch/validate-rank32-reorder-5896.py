#!/usr/bin/env python3
"""Validate the first-post-Reorder heap using the unchanged replay contract."""
from pathlib import Path
source = Path(__file__).with_name('validate-rank32-choice-5890.py')
code = source.read_text().replace('20260908-5883-rank32-choice-frontier', '20260908-5891-rank32-reorder-frontier')
code = code.replace("row[4] == '116746'", "row[4] == '202958'")
code = code.replace('Heap snapshot follows decision61017804', 'Heap snapshot follows first post-Reorder decision61013974')
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})

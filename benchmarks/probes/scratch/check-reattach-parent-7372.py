#!/usr/bin/env python3
"""Actual optimized parent silently duplicates maps; required invariant fails."""
from pathlib import Path
base=Path(__file__).with_name('check-lucky-root-parent-7364.py')
code=base.read_text()
for before,after in {
    '20260911-7364-lucky-root-parent':'20260911-7372-reattach-parent',
    "test=Path(__file__).with_name('lucky-root-consumer-7364.cpp')":"test=root/'.claude/worktrees/runtime-lucky-reattach-7371/tests/test_heuristics_reattach.cpp'",
    'test_lucky_root_consumer':'test_heuristics_reattach',
    'Actual DefaultPolicy early Lucky with supplied H leaves proved roots queued. Root counts1/5/29, both import directions and physical-fold modes reproduce; desired assertion fails before repair.':'Actual optimized parent duplicates compact order/inverse on fresh H attachment. Eight fixtures and three repeated attachments expose48 changed images; required invariant fails. Separate7368 actual Debug failure catches the duplicate at insertion.',
}.items():
    assert before in code,before;code=code.replace(before,after)
exec(compile(code,str(base),'exec'),dict(__file__=str(Path(__file__)),__name__='__main__'))

#!/usr/bin/env python3
"""Move the neutral heap observer to the first post-Reorder decision."""
from pathlib import Path
import re
source = Path(__file__).with_name('prepare-rank32-choice-5883.py')
code = source.read_text()
changes = {
    '20260908-5883-rank32-choice-frontier': '20260908-5891-rank32-reorder-frontier',
    'tactix-5883-': 'tactix-5891-',
    '5883-choice-frontier': '5891-reorder-frontier',
    "SAT_EVENT_TRACE_HEAP_SET_DECISION='61017804'": "SAT_EVENT_TRACE_HEAP_SET_DECISION='61013974'",
    "SAT_CHOICE_STATE_VARIABLE='116746'": "SAT_CHOICE_STATE_VARIABLE='202958'",
    'Reduction49 clause metadata and selected victims match exactly. Distinguish post-Reduce/Reorder heap scores/pose from propagation assignment drift immediately before first unequal choice61017805.':
        '5883 proves shared heap scores/assignment flags match before first unequal choice,but14282 positions differ andTactix retains assigned202958. Capture full heap immediately after first post-Reorder decision61013974 to separate incoming Reorder pose from later lazy-pop drift;trace202958 assignment provenance.',
    'Complete physical heap after common decision61017804;all116746 assignment/reason states at225987.':
        'Complete physical heap after first common post-Reorder decision61013974;all202958 assignment/reason states at225987.',
    'Cached native lacks complete heap and assignment reasons at the newly isolated decision boundary.':
        'Cached native5883 lacks complete first post-Reorder heap and202958 assignment provenance.',
}
code = re.sub('|'.join(re.escape(k) for k in sorted(changes, key=len, reverse=True)), lambda m: changes[m.group()], code)
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})

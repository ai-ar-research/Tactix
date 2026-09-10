#!/usr/bin/env python3
from pathlib import Path
source = Path(__file__).with_name('validate-inprocessing-profiles-6817.py')
code = source.read_text()
for before, after in {
    '20260910-6815-inprocessing-release-profiles':'20260910-6945-congruence-release-profiles',
    'solver-summary-6817.json':'solver-summary-6947.json',
    'range(4)':'range(2)',
    "((32,'candidate'),(32,'kissat'),(39,'candidate'),(39,'kissat'))":
        "((47,'candidate'),(47,'kissat'))",
    'All four sampled arms':'Both sampled arms',
    'One four-task199Hz':'One two-task199Hz',
    'Rank32 full conflicts/decisions match native, not proof of all-operation identity; Rank39 work differs.':
        'Rank47 work differs across arms; samefive closure count does not prove equal internal operation counts.',
}.items():
    assert before in code,before
    code = code.replace(before,after)
exec(compile(code,str(source),'exec'),{'__file__':str(Path(__file__)),'__name__':'__main__'})

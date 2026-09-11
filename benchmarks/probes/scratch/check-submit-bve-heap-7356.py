#!/usr/bin/env python3
"""Same own-Release proof/work gate, plus five trace bindings on final rows."""
from pathlib import Path
base=Path(__file__).with_name('check-submit-root-decision-trace-7349.py')
code=base.read_text().replace('7349-root-decision-trace','7356-bve-heap')
anchor="{'decisions','macro'}";assert anchor in code
code=code.replace(anchor,"{'decisions','macro','active','inprocess','heap'}")
exec(compile(code,str(base),'exec'),dict(__file__=str(Path(__file__)),__name__='__main__'))

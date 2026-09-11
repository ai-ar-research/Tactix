#!/usr/bin/env python3
"""Reuse exact whole Release replay, with all four completed trace bindings."""
from pathlib import Path
base=Path(__file__).with_name('check-submit-root-decision-trace-7349.py')
code=base.read_text().replace('7349-root-decision-trace','7353-root-phase-trace')
anchor="{'decisions','macro'}"; assert anchor in code
code=code.replace(anchor,"{'decisions','macro','active','inprocess'}")
exec(compile(code,str(base),'exec'),dict(__file__=str(Path(__file__)),__name__='__main__'))

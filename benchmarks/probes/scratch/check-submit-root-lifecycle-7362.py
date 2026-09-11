#!/usr/bin/env python3
"""Default read-only exact own-Release gate; submit only with --submit once."""
from pathlib import Path
base = Path(__file__).with_name('check-submit-root-decision-trace-7349.py')
code = base.read_text().replace('7349-root-decision-trace', '7362-root-lifecycle')
anchor = "{'decisions','macro'}"
assert anchor in code
code = code.replace(anchor, "{'decisions','macro','active','inprocess','heap','queue','roots','deact'}")
exec(compile(code, str(base), 'exec'), dict(__file__=str(Path(__file__)), __name__='__main__'))

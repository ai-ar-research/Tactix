#!/usr/bin/env python3
"""Submit only the new preclean gates; preserve every predecessor launch."""
from pathlib import Path
source = Path(__file__).with_name('submit-forward-caller-5860.py')
code = source.read_text().replace("'20260908-5855-forward-caller-BC-gate',\n             '20260908-5856-forward-caller-rank31-prefix',\n             '20260908-5857-forward-caller-rank32-prefix'", "'20260908-5878-forward-preclean-BC-gate',\n             '20260908-5879-forward-preclean-rank31-prefix'")
code = code.replace('20260908-5858-forward-caller-ranks1-40', '20260908-5880-forward-preclean-ranks1-40')
code = code.replace('Independent5849 successor', 'Independent5870 successor')
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})

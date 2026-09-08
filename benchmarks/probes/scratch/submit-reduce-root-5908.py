#!/usr/bin/env python3
"""Submit only freshly pinned5900 gates; preserve every ongoing runner."""
from pathlib import Path
source=Path(__file__).with_name('submit-forward-caller-5860.py')
code=source.read_text().replace("'20260908-5855-forward-caller-BC-gate',\n             '20260908-5856-forward-caller-rank31-prefix',\n             '20260908-5857-forward-caller-rank32-prefix'", "'20260908-5906-reduce-root-BC-gate',\n             '20260908-5907-reduce-root-rank32-prefix'")
code=code.replace('20260908-5858-forward-caller-ranks1-40','20260908-5908-reduce-root-ranks1-40').replace('Independent5849 successor','Independent5900 successor on5849;regressing5870 remains separate')
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})

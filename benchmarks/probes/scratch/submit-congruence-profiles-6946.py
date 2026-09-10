#!/usr/bin/env python3
from pathlib import Path
source = Path(__file__).with_name('submit-inprocessing-profiles-6816.py')
code = source.read_text()
before = '20260910-6815-inprocessing-release-profiles'
assert before in code
code = code.replace(before,'20260910-6945-congruence-release-profiles')
exec(compile(code,str(source),'exec'),{'__file__':str(Path(__file__)),'__name__':'__main__'})

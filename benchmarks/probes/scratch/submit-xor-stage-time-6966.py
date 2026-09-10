#!/usr/bin/env python3
from pathlib import Path
source = Path(__file__).with_name('submit-inprocessing-profiles-6816.py')
code = source.read_text()
before = '20260910-6815-inprocessing-release-profiles'
assert before in code
code = code.replace(before,'20260910-6965-xor-stage-time')
before = "for record in m['unprofiled_records'].values():assert sha(root/record['path'])==record['sha256']"
assert before in code
code = code.replace(before,"for records in m['work_oracles'].values():\n    for record in records.values(): assert sha(root/record['path']) == record['sha256']")
exec(compile(code,str(source),'exec'),{'__file__':str(Path(__file__)),'__name__':'__main__'})

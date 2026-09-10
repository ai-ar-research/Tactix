#!/usr/bin/env python3
from pathlib import Path
source=Path(__file__).with_name('submit-material-runtime-6593.py')
code=source.read_text()
for before,after in {'20260909-6592-rank50-material-three-wave-runtime':'20260909-6741-rank46-watermark-runtime',"int(row['rank'])==50":"int(row['rank'])==46","('pending_validation','checked_SAT')":"('pending_validation','checked_UNSAT')"}.items():
    assert before in code;code=code.replace(before,after)
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})

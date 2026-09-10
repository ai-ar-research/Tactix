#!/usr/bin/env python3
from pathlib import Path
import subprocess
import sys
root=Path(__file__).resolve().parents[3]
subprocess.run([sys.executable,str(root/'tools/slurm/session_deadline.py'),'--deadline','2026-09-10T06:00:00-04:00'],check=True)
source=Path(__file__).with_name('submit-material-runtime-6593.py')
code=source.read_text()
for before,after in {'20260909-6592-rank50-material-three-wave-runtime':'20260909-6754-rank38-watermark-cost-runtime',"int(row['rank'])==50":"int(row['rank'])==38","('pending_validation','checked_SAT')":"('pending_validation','checked_UNSAT')","oracle=old if r['arm']=='previous' else records[0]":"oracle=old"}.items():
    assert before in code;code=code.replace(before,after)
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})

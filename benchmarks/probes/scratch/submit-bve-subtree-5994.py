#!/usr/bin/env python3
"""Submit only the frozen second-pop subtree diagnostic."""
from pathlib import Path
source=Path(__file__).with_name('submit-bve-repeat-5989.py')
code=source.read_text().replace("'repeat': ['20260908-5988-bve-repeat-heap']", "'subtree': ['20260908-5992-bve-subtree-ancestry']")
code=code.replace('Diagnostic-only5985 observer','Retargeted validated5949 observer')
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})

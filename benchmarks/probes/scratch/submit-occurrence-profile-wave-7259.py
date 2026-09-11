#!/usr/bin/env python3
"""Submit one complete-Eliminate profile pair after prior solver outcomes."""
from pathlib import Path
base = Path(__file__).with_name('submit-vivify-compact-profile-wave-7152.py')
code = base.read_text().replace('validate-vivify-compact-profiles-7152.py',
                               'validate-occurrence-profiles-7259.py')
exec(compile(code,str(base),'exec'),{'__file__':str(Path(__file__)),'__name__':'__main__'})

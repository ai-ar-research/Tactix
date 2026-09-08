#!/usr/bin/env python3
"""Submit only the fresh first-BVE causal campaign."""
from pathlib import Path
source=Path(__file__).with_name('submit-second-forward-formulas-5933.py')
code=source.read_text().replace("('20260908-5931-second-forward-begin-formula','20260908-5932-second-forward-end-formula')",
                              "('20260908-5937-first-bve-frontier',)")
code=code.replace('Read-only formula observers on frozen5920/native pins','Read-only formula andBVE observers on frozen5900/native pins')
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})

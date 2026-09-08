#!/usr/bin/env python3
"""Submit the fresh selected-heap diagnostic only."""
from pathlib import Path
source = Path(__file__).with_name('submit-second-forward-formulas-5933.py')
code = source.read_text().replace("('20260908-5931-second-forward-begin-formula','20260908-5932-second-forward-end-formula')", "('20260908-5945-bve-heap-frontier',)")
code = code.replace('Read-only formula observers on frozen5920/native pins','Selected heap observer on unchanged5900 and fresh native diagnostic pin; existing campaigns unchanged')
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})

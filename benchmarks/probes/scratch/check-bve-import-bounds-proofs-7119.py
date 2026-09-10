#!/usr/bin/env python3
"""Repeat unchanged strict definition/Sweep proof controls on the successor."""
from pathlib import Path
import sys
assert sys.argv[1:] in (['definition'], ['sweep'])
sweep = sys.argv[1] == 'sweep'
source = Path(__file__).with_name('check-bve-import-sweep-proofs-7115.py' if sweep
                                else 'check-bve-import-proofs-7113.py')
code = source.read_text()
code = code.replace('20260910-7115-bve-import-sweep-proofs', '20260910-7120-bve-import-bounds-sweep-proofs')
code = code.replace('20260910-7113-bve-import-proofs', '20260910-7119-bve-import-bounds-proofs')
code = code.replace("('candidate', '7110-bve-import')", "('candidate', '7116-bve-import-bounds')")
code = code.replace('7087/7110', '7087/7116')
exec(compile(code, str(source), 'exec'), {'__file__': str(Path(__file__)), '__name__': '__main__'})

#!/usr/bin/env python3
"""Require all66 checked and cleaned B28+C5 records before closing the gate."""
from pathlib import Path
source=Path(__file__).with_name('seal-propagation-gate-5656.py').read_text()
source=source.replace('20260908-5651-propagation-cursor-BC-gate','20260908-5727-root-reuse-BC-gate')
exec(compile(source,__file__,'exec'))

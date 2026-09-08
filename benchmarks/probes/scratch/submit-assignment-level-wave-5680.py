#!/usr/bin/env python3
"""Advance exact-work waves after solver outcomes, with asynchronous checks."""
from pathlib import Path
source = Path(__file__).with_name('submit-propagation-wave-5653.py').read_text()
source = source.replace('20260908-5652-propagation-cursor-rank39-runtime','20260908-5679-assignment-level-rank39-runtime')
source = source.replace('20260908-5651-propagation-cursor-BC-gate','20260908-5678-assignment-level-BC-gate')
exec(compile(source,__file__,'exec'))

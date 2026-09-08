#!/usr/bin/env python3
"""Require three exact-work/proof pairs before scoring the maximum early exit."""
from pathlib import Path
source = Path(__file__).with_name('snapshot-propagation-runtime-5655.py').read_text()
source = source.replace('20260908-5652-propagation-cursor-rank39-runtime','20260908-5679-assignment-level-rank39-runtime')
source = source.replace('5652 solver results pending','5679 solver results pending')
exec(compile(source,__file__,'exec'))

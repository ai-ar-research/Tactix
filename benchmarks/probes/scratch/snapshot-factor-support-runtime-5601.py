#!/usr/bin/env python3
"""Retain the changed-work three-wave contract and within-arm identity checks."""
from pathlib import Path
source = Path(__file__).with_name('snapshot-sweep-retained-runtime-5526.py').read_text()
source = source.replace('20260908-5517-sweep-retained-rank32-runtime', '20260908-5599-factor-support-rank32-runtime')
source = source.replace('5517 solver results pending', '5599 solver results pending')
exec(compile(source, __file__, 'exec'))

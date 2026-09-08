#!/usr/bin/env python3
"""Preserve the three-pair changed-work and precision acceptance contract."""
from pathlib import Path
source=Path(__file__).with_name('snapshot-sweep-retained-runtime-5526.py').read_text()
source=source.replace('20260908-5517-sweep-retained-rank32-runtime','20260908-5629-congruence-index-rank33-runtime')
source=source.replace('5517 solver results pending','5629 solver results pending')
exec(compile(source,__file__,'exec'))

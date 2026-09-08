#!/usr/bin/env python3
"""Score all three independent changed-work timing pairs with explicit checks."""
from pathlib import Path
source=Path(__file__).with_name('snapshot-factor-support-runtime-5601.py').read_text()
source=source.replace('20260908-5599-factor-support-rank32-runtime','20260908-5728-root-reuse-rank33-runtime')
source=source.replace('5599 solver results pending','5728 solver results pending')
exec(compile(source,__file__,'exec'))

#!/usr/bin/env python3
"""Validate the second-pass initial heap against full parent replay."""
from pathlib import Path
source=Path(__file__).with_name('compare-factor-heap-5573.py').read_text()
source=source.replace('20260908-5571-rank32-factor-heap','20260908-5580-rank32-factor-entry')
source=source.replace('5571 pending','5580 pending')
source=source.replace('Snapshot is after accepted grid4824.',
    'Snapshot is at the second Factor pass initial stage, after4693 previous grids.')
exec(compile(source,__file__,'exec'))

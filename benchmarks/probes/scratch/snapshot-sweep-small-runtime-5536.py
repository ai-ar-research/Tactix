#!/usr/bin/env python3
"""Preserve the changed-work, within-arm replication contract for5522."""
from pathlib import Path
source = Path(__file__).with_name('snapshot-sweep-retained-runtime-5526.py').read_text()
source = source.replace('20260908-5517-sweep-retained-rank32-runtime',
                        '20260908-5531-sweep-small-rank32-runtime')
source = source.replace('5517 solver results pending', '5531 solver results pending')
exec(compile(source, __file__, 'exec'))

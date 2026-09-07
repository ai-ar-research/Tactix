#!/usr/bin/env python3
"""Keep the failed reference selector separate from the valid Tactix comparison."""
import hashlib
import json
from pathlib import Path

C = Path(__file__).resolve().parent
raw = {arm:(C/'traces'/(arm+'.tsv')).read_bytes()
       for arm in ('core','sequential','kissat')}
for arm in ('core','sequential'):
    assert b'# truncated' not in raw[arm]
    rows=[line.split(b'\t') for line in raw[arm].splitlines() if not line.startswith(b'#')]
    assert int(rows[-1][3]) == 1
assert raw['core'] == raw['sequential']
print(json.dumps(dict(core_sequential_byte_identical=True,
    tactix_rows=len(rows),terminal_conflict=1,
    kissat_valid=False,reason='SNAPSHOT_ONLY is not implemented in this Kissat pin; import phase_save fills the 10000-row cap.',
    kissat_truncated=b'# truncated' in raw['kissat'],
    trace_sha256={arm:hashlib.sha256(data).hexdigest() for arm,data in raw.items()}),indent=2))

#!/usr/bin/env python3
"""Generate new checkpoint summaries from their assertion-bearing comparators."""
import subprocess
import sys
from pathlib import Path

root = Path(__file__).resolve().parents[3]
for name in ('20260907-5348-choice-state', '20260907-5349-choice-deactivation',
             '20260907-5350-bve-frontier'):
    campaign = root / 'benchmarks/runs' / name
    target = campaign / 'summary.json'
    assert not target.exists(), target
    result = subprocess.run([sys.executable, str(campaign / 'compare.py')],
                            check=True, capture_output=True, text=True)
    target.write_text(result.stdout)
    print(name, 'validated')

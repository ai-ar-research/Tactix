#!/usr/bin/env python3
"""Seal the newly completed schedule result once."""
import subprocess
import sys
from pathlib import Path

root = Path(__file__).resolve().parents[3]
campaign = root / 'benchmarks/runs/20260907-5351-bve-schedule'
target = campaign / 'summary.json'
assert not target.exists()
result = subprocess.run([sys.executable, str(campaign / 'compare.py')],
                        check=True, capture_output=True, text=True)
target.write_text(result.stdout)
print('5351 validated')

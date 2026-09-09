#!/usr/bin/env python3
"""Measure all50 after the proven general repair; pending checks do not block."""
from pathlib import Path
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
import fixed_runtime as f
c=root/'benchmarks/runs/20260909-6239-substitute-retry-ranks1-50'
f.prepare(c,root/'benchmarks/runs/20260909-6231-substitute-retry-build',
    root/'build/pin-6231-substitute-retry-release/tactix',root/'build/pin-6231-substitute-retry-bc/tactix')
print(c)

#!/usr/bin/env python3
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root/'tools/bench'))
import fixed_runtime as f
c = root/'benchmarks/runs/20260909-6278-vivify-binary-ranks1-50'
f.prepare(c, root/'benchmarks/runs/20260909-6269-vivify-binary-retry-build',
    root/'build/pin-6269-vivify-binary-retry-release/tactix', root/'build/pin-6269-vivify-binary-retry-bc/tactix')
print(c)

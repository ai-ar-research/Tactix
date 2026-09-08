#!/usr/bin/env python3
"""Copy the exact retained native diagnostic lineage before observer editing."""
import hashlib
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
parent = root / 'tools/solvers/variants/kissat-imply-fast-5414'
assert hashlib.sha256((parent/'build/kissat').read_bytes()).hexdigest() == '849db948ead9166917d9598e02cea6b34703a0f87c7bc5b437a704b24099dc16'
source = root/'tools/solvers/variants/kissat-bve-heap-observer-5944'
shutil.copytree(parent, source, symlinks=True,
                ignore=shutil.ignore_patterns('.git','build','__pycache__'))
print(source)

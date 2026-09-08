#!/usr/bin/env python3
"""Preserve cancelled5945 and its pin; create a lifecycle-correct observer."""
from pathlib import Path
import shutil
root=Path(__file__).resolve().parents[3]
shutil.copytree(root/'tools/solvers/variants/kissat-bve-heap-observer-5944',
                root/'tools/solvers/variants/kissat-bve-heap-lifecycle-5948',
                symlinks=True,ignore=shutil.ignore_patterns('.git','build','__pycache__'))

#!/usr/bin/env python3
"""CLI adapter only; execute the unchanged, source-bound root-fold unit fixture."""
import os
from pathlib import Path
import sys

args=sys.argv[1:]; mode='drat'
if args and args[0]=='--lrat': mode='lrat'; args=args[1:]
assert len(args)==2
exe=Path(__file__).with_name('root-fixture')
os.execv(exe,[str(exe),*args,mode,'fold'])

#!/usr/bin/env python3
"""Restore copied-source examples and retain failed7301 configure logs."""
import json
from pathlib import Path
import shutil
root = Path(__file__).resolve().parents[3]
source = root / '.claude/worktrees/runtime-clause-realloc-7301'
parent = root / '.claude/worktrees/runtime-dense-realloc-7267'
assert not (source / 'examples').exists()
assert (parent / 'examples').is_dir()
shutil.copytree(parent / 'examples', source / 'examples', symlinks=True)
base = Path(__file__).with_name('build-clause-realloc-7301.py')
code = base.read_text()
code = code.replace('20260911-7301-clause-realloc-build', '20260911-7302-clause-realloc-build')
code = code.replace('build/pin-7301-clause-realloc-', 'build/pin-7302-clause-realloc-')
exec(compile(code, str(base), 'exec'), {'__file__': str(Path(__file__)), '__name__': '__main__'})

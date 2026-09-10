#!/usr/bin/env python3
"""Retain the checked runtime protocol; scope one declared solver environment."""
import csv
import importlib.util
import json
from pathlib import Path
import sys

c = Path(sys.argv[1]).resolve()
sys.path.insert(0, str(c / 'pins'))
from solver_environment import with_solver_environment

m = json.loads((c / 'manifest.json').read_text())
with (c / 'matrix.tsv').open(newline='') as stream:
    rows = list(csv.DictReader(stream, delimiter='\t'))
row = rows[int(sys.argv[2])]
assert int(row['task']) == int(sys.argv[2])
arm = m['arms'][row['arm']]
spec = importlib.util.spec_from_file_location('runtime_protocol', c / 'pins/runtime_task.py')
h = importlib.util.module_from_spec(spec)
spec.loader.exec_module(h)
h.measured = with_solver_environment(h.measured, c / arm['path'], arm['environment'])
raise SystemExit(h.main())

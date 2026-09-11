#!/usr/bin/env python3
"""Original Tactix under external debugger; unchanged comparison arm."""
import importlib.util
import os
from pathlib import Path
import sys
assert os.environ.get('SLURM_JOB_ID'), 'real solvers require SLURM'
c = Path(sys.argv[1]).resolve()
def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module
protocol = load('runtime_protocol', c/'pins/runtime_task.py')
observer = load('root_gdb_task', c/'pins/root_gdb_task.py')
observer.install_observer(protocol, c, sys.argv[2], c/'pins/candidate')
raise SystemExit(protocol.main())

#!/usr/bin/env python3
"""Select an existing read-only memory observer without changing solver pins."""
import importlib.util
import os
from pathlib import Path
import sys

assert os.environ.get('SLURM_JOB_ID')
c = Path(sys.argv[1]).resolve()
spec = importlib.util.spec_from_file_location('runtime_protocol', c / 'pins/runtime_task.py')
h = importlib.util.module_from_spec(spec); spec.loader.exec_module(h)
original_measured = h.measured

def observe(command, stdout, stderr, metrics, timeout):
    if str(c / 'pins/candidate') not in set(map(str, command)):
        return original_measured(command, stdout, stderr, metrics, timeout)
    path = c / 'logs/slot-memory.tsv'
    os.environ['TACTIX_SLOT_MEMORY_TRACE'] = str(path)
    try:
        result = original_measured(command, stdout, stderr, metrics, timeout)
    finally:
        del os.environ['TACTIX_SLOT_MEMORY_TRACE']
    result['memory_trace'] = dict(sha256=h.sha256(path), bytes=path.stat().st_size)
    return result

h.measured = observe
raise SystemExit(h.main())

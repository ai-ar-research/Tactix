#!/usr/bin/env python3
"""Retained checked runtime protocol with a solver-only sampling wrapper."""
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys

assert os.environ.get('SLURM_JOB_ID')
c = Path(sys.argv[1]).resolve()
spec = importlib.util.spec_from_file_location('runtime_protocol', c / 'pins/runtime_task.py')
h = importlib.util.module_from_spec(spec); spec.loader.exec_module(h)
original_measured = h.measured
solvers = {str(c / 'pins' / a) for a in ('candidate', 'previous')}
m = json.loads((c / 'manifest.json').read_text())
task = int(sys.argv[2])

def profiled(command, stdout, stderr, metrics, timeout):
    if not solvers.intersection(map(str, command)):
        return original_measured(command, stdout, stderr, metrics, timeout)
    command = list(command)
    # Replace the protocol's optional perf-stat wrapper, never its bound solver.
    if command[:2] == ['perf', 'stat']:
        command = command[command.index('--') + 1:]
    data = metrics.parent / 'profile.data'
    measured_command = ['perf', 'record', '--no-buildid-cache', '-o', data,
                        '-e', m['profile']['event'], '-F', str(m['profile']['frequency']),
                        '--call-graph', m['profile']['call_graph'], '--', *command]
    result = original_measured(measured_command, stdout, stderr, metrics, timeout)
    profile = dict(command=list(map(str, measured_command)), reports={})
    for name, args in [('self', ['--no-children', '--call-graph', 'none']),
                       ('callpaths', ['--children', '--call-graph', 'graph,0.5,caller'])]:
        target = c / 'profiles' / f'{task:03d}-{name}.txt'
        with target.open('w') as out:
            r = subprocess.run(['perf', 'report', '--stdio', '-i', data, '--sort', 'dso,symbol',
                                '--percent-limit', '0.1', *args], stdout=out, stderr=subprocess.PIPE, text=True)
        profile['reports'][name] = dict(exit=r.returncode, stderr=r.stderr, sha256=h.sha256(target))
        assert r.returncode == 0
    saved = c / 'profiles' / f'{task:03d}.perf.data.xz'
    with saved.open('wb') as out: subprocess.run(['xz', '-T1', '-c', data], stdout=out, check=True)
    profile.update(bytes=data.stat().st_size, sha256=h.sha256(data),
                   compressed_bytes=saved.stat().st_size, compressed_sha256=h.sha256(saved))
    result['profile'] = profile
    return result

h.measured = profiled
raise SystemExit(h.main())

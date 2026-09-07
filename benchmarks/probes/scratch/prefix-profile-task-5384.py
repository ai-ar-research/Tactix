#!/usr/bin/env python3
"""SLURM-only read-only sampled profile, preserving raw data and symbol reports."""
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

c = Path(__file__).resolve().parents[1]
root = c.parents[2]
assert os.environ.get('SLURM_JOB_ID')
spec = importlib.util.spec_from_file_location('runtime_helpers', c / 'pins/runtime_task.py')
h = importlib.util.module_from_spec(spec); spec.loader.exec_module(h)
m = json.loads((c / 'manifest.json').read_text())
for rel, expected in m['hashes'].items(): assert h.sha256(c / rel) == expected
arm = ('tactix', 'kissat')[int(sys.argv[1])]
for key in list(os.environ):
    if key.startswith(('TACTIX_', 'KISSAT_', 'SAT_')): del os.environ[key]
if arm == 'tactix': os.environ['TACTIX_STOP_AFTER_CONFLICT'] = '1000000'
scratch = Path(tempfile.mkdtemp(prefix='tactix-prefix-profile-', dir=os.environ.get('SLURM_TMPDIR', '/tmp')))
result = None
try:
    cnf = scratch / 'input.cnf'; proof = scratch / 'partial.drat'; data = scratch / 'perf.data'
    compressed = root / m['input']
    assert h.sha256(compressed) == m['input_sha256_compressed']
    with cnf.open('wb') as out: subprocess.run(['xz', '-dc', compressed], stdout=out, check=True)
    affinity = sorted(os.sched_getaffinity(0))
    context = h.runtime_context(affinity)
    command = ['taskset', '-c', str(affinity[0]), c / 'pins' / arm]
    if arm == 'kissat': command += ['-v', '--conflicts=1000000']
    command += [cnf, proof]
    command = ['perf', 'record', '--no-buildid-cache', '-o', data, '-e', m['profile']['event'],
               '-F', str(m['profile']['frequency']), '--call-graph', m['profile']['call_graph'], '--', *command]
    stdout = c / 'logs' / (arm + '.out'); stderr = c / 'logs' / (arm + '.err')
    measurement = h.measured(command, stdout, stderr, scratch / 'time.txt', 600)
    verdict, statistics = h.solver_summary(stdout)
    reports = {}
    for name, args in [('self', ['--no-children', '--call-graph', 'none']),
                       ('callpaths', ['--children', '--call-graph', 'graph,0.5,caller'])]:
        target = c / 'profiles' / (arm + '-' + name + '.txt')
        with target.open('w') as out:
            p = subprocess.run(['perf', 'report', '--stdio', '-i', data, '--sort', 'dso,symbol',
                                '--percent-limit', '0.1', *args], stdout=out, stderr=subprocess.PIPE, text=True)
        reports[name] = dict(exit=p.returncode, stderr=p.stderr, sha256=h.sha256(target))
        assert p.returncode == 0
    saved = c / 'profiles' / (arm + '.perf.data.xz')
    with saved.open('wb') as out: subprocess.run(['xz', '-T1', '-c', data], stdout=out, check=True)
    result = dict(arm=arm, job=os.environ['SLURM_JOB_ID'], node=os.uname().nodename,
                  binary_sha256=h.sha256(c / 'pins' / arm), command=list(map(str, command)),
                  measurement=measurement, verdict=verdict, statistics=statistics,
                  runtime_context=context, reports=reports,
                  profile=dict(bytes=data.stat().st_size, sha256=h.sha256(data),
                               compressed_bytes=saved.stat().st_size, compressed_sha256=h.sha256(saved)),
                  partial_certificate_bytes=proof.stat().st_size, partial_certificate_sha256=h.sha256(proof),
                  limitations=m['limitations'])
    assert measurement['exit_code'] == 0 and not measurement['timed_out'] and verdict == 'UNKNOWN'
finally:
    # This exact mkdtemp directory is owned by this task; durable profiles precede cleanup.
    shutil.rmtree(scratch)
    print('scratch_deleted', not scratch.exists())
    if result:
        result['scratch_deleted'] = not scratch.exists()
        (c / 'results' / (arm + '.json')).write_text(json.dumps(result, indent=2) + '\n')

#!/usr/bin/env python3
"""Trace-free, proof-on stopped-prefix cost diagnostic; no verdict acceptance."""
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
os.environ['TACTIX_RUNTIME_SAMPLE_VM'] = '1'
scratch = Path(tempfile.mkdtemp(prefix='tactix-prefix-cost-', dir=os.environ.get('SLURM_TMPDIR', '/tmp')))
result = None
try:
    cnf = scratch / 'input.cnf'; proof = scratch / 'partial.drat'
    compressed = root / m['input']
    assert h.sha256(compressed) == m['input_sha256_compressed']
    with cnf.open('wb') as out: subprocess.run(['xz', '-dc', compressed], stdout=out, check=True)
    affinity = sorted(os.sched_getaffinity(0))
    context = h.runtime_context(affinity)
    command = ['taskset', '-c', str(affinity[0]), c / 'pins' / arm]
    if arm == 'kissat': command += ['-v', '--conflicts=1000000']
    command += [cnf, proof]
    probe = subprocess.run(['perf', 'stat', '-e', 'cycles', '--', 'true'], capture_output=True, text=True)
    context['perf_probe'] = dict(exit=probe.returncode, stderr=probe.stderr)
    if probe.returncode == 0:
        command = ['perf', 'stat', '-x', '\t', '-o', scratch / 'perf.tsv', '-e',
                   'task-clock,cycles,instructions,context-switches,cpu-migrations,page-faults', '--', *command]
    stdout = c / 'logs' / (arm + '.out'); stderr = c / 'logs' / (arm + '.err')
    measurement = h.measured(command, stdout, stderr, scratch / 'time.txt', 600)
    verdict, statistics = h.solver_summary(stdout)
    context['perf_statistics'] = h.tail(scratch / 'perf.tsv', 50)
    context['end'] = h.runtime_context(affinity)
    result = dict(arm=arm, node=os.uname().nodename, binary_sha256=h.sha256(c / 'pins' / arm),
                  command=list(map(str, command)), measurement=measurement, verdict=verdict,
                  statistics=statistics, runtime_context=context,
                  partial_certificate_bytes=proof.stat().st_size, partial_certificate_sha256=h.sha256(proof),
                  validity='Intentional UNKNOWN at one million conflicts; no final model/proof acceptance.',
                  limitations=m['limitations'])
    assert measurement['exit_code'] == 0 and not measurement['timed_out']
finally:
    shutil.rmtree(scratch)
    print('scratch_deleted', not scratch.exists())
    if result:
        result['scratch_deleted'] = not scratch.exists()
        (c / 'results' / (arm + '.json')).write_text(json.dumps(result, indent=2) + '\n')

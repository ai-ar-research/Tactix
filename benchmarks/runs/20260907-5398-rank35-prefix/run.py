#!/usr/bin/env python3
"""One bounded proof-on observer per SLURM task; never an acceptance timing."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

c = Path(__file__).resolve().parent
root = c.parents[2]
arm = ('candidate', 'kissat')[int(sys.argv[1])]
prefix = 'KISSAT' if arm == 'kissat' else 'TACTIX'
assert os.environ.get('SLURM_JOB_ID')
m = json.loads((c / 'manifest.json').read_text())
def sha(p):
    h = hashlib.sha256()
    with p.open('rb') as stream:
        for block in iter(lambda: stream.read(1048576), b''):
            h.update(block)
    return h.hexdigest()
for rel, expected in m['hashes'].items():
    assert sha(c / rel) == expected, rel
scratch = Path(tempfile.mkdtemp(prefix='tactix-rank27-prefix-',
                               dir=os.environ.get('SLURM_TMPDIR', '/tmp')))
env = {k: v for k, v in os.environ.items()
       if not k.startswith(('TACTIX_', 'KISSAT_', 'SAT_'))}
env.update({prefix + '_INPROCESS_BOUNDARY_TRACE': str(c / 'traces' / (arm + '.boundaries.tsv')),
            'SAT_INPROCESS_BOUNDARY_CONFLICT_FROM': '0',
            prefix + '_DECISION_STREAM_TRACE': str(c / 'traces' / (arm + '.decisions.tsv')),
            prefix + '_DECISION_PREFIX_TRACE': str(c / 'traces' / (arm + '.prefix.tsv')),
            'SAT_DECISION_PREFIX_CONFLICT': str(m['stop_conflict']),
            'SAT_DECISION_PREFIX_COMMON': '1', 'SAT_DECISION_PREFIX_STOP': '1'})
r = dict(arm=arm, slurm_job=os.environ['SLURM_JOB_ID'],
         node=os.uname().nodename, binary_sha256=sha(c / 'pins' / arm),
         purpose='Stopped diagnostic; no checked final verdict or timing claim.')
try:
    compressed = root / m['input']
    assert sha(compressed) == m['input_sha256_compressed']
    cnf, proof = scratch / 'input.cnf', scratch / 'partial.drat'
    with cnf.open('wb') as out:
        subprocess.run(['xz', '-dc', compressed], stdout=out, check=True)
    command = ['timeout', '600', 'taskset', '-c', str(min(os.sched_getaffinity(0))),
               str(c / 'pins' / arm), str(cnf), str(proof)]
    r['command'] = command
    with (c / 'logs' / (arm + '.out')).open('wb') as out:
        p = subprocess.run(command, stdout=out, stderr=subprocess.STDOUT, env=env)
    r['exit_code'] = p.returncode
    r['partial_certificate'] = dict(bytes=proof.stat().st_size, sha256=sha(proof)) if proof.exists() else None
    r['trace_sha256'] = {p.name: sha(p) for p in (c / 'traces').glob(arm + '.*')}
finally:
    shutil.rmtree(scratch)
    r['scratch_deleted'] = not scratch.exists()
    (c / 'results' / (arm + '.json')).write_text(json.dumps(r, indent=2) + '\n')
assert r['exit_code'] == 0, 'Unexpected solver exit: inspect before further performance work'
assert (c / 'traces' / (arm + '.prefix.tsv')).exists(), 'Requested stop was not observed'
print(arm, 'bounded stop and cleanup complete')

#!/usr/bin/env python3
"""One bounded proof-on observer per SLURM task; never an acceptance timing."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import signal
import subprocess
import sys
import tempfile

c = Path(__file__).resolve().parent
root = c.parents[2]
task = int(sys.argv[1])
arm = ('candidate', 'kissat')[task % 2]
tag = f'{task // 2:02d}-' + arm
point = (2002, 8033, 13563)[task // 2]
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
scratch = Path(tempfile.mkdtemp(prefix='tactix-5416-' + os.environ['SLURM_JOB_ID'] + '-',
                               dir=os.environ.get('SLURM_TMPDIR', '/tmp')))
env = {k: v for k, v in os.environ.items()
       if not k.startswith(('TACTIX_', 'KISSAT_', 'SAT_'))}
env.update({prefix + '_INPROCESS_BOUNDARY_TRACE': str(c / 'traces' / (tag + '.boundaries.tsv')),
            'SAT_INPROCESS_BOUNDARY_CONFLICT_FROM': '0',
            prefix + '_DECISION_STREAM_TRACE': str(c / 'traces' / (tag + '.decisions.tsv')),
            prefix + '_DECISION_PREFIX_TRACE': str(c / 'traces' / (tag + '.prefix.tsv')),
            'SAT_DECISION_PREFIX_CONFLICT': str(m['stop_conflict']),
            'SAT_DECISION_PREFIX_COMMON': str(m.get('common_decisions', 1)), 'SAT_DECISION_PREFIX_STOP': '1'})
env['SAT_DECISION_STREAM_CONFLICT_FROM'] = str(m['stream_from'])
env[prefix + '_CHOICE_STATE_TRACE'] = str(c / 'traces' / (tag + '.choice.tsv'))
env['SAT_CHOICE_STATE_CONFLICT'] = str(m['choice_conflict'])
env['SAT_CHOICE_STATE_VARIABLE'] = str(m['choice_variable'])
env[prefix + '_MACRO_BOUNDARY_TRACE'] = str(c / 'traces' / (tag + '.macro.tsv'))
env['SAT_MACRO_BOUNDARY_CONFLICT_FROM'] = '13000'
env.update({
    'SAT_EVENT_TRACE_HEAP_SET_PATH': str(c / 'traces' / (tag + '.heap.tsv')),
    'SAT_EVENT_TRACE_HEAP_SET_CONFLICT': '13563',
    'SAT_EVENT_TRACE_HEAP_SET_DECISION': '17798663',
    'SAT_EVENT_TRACE_HEAP_SET_AFTER_DECISION': '1',
})
def interrupted(signum, frame):
    raise SystemExit(128 + signum)
signal.signal(signal.SIGTERM, interrupted)
signal.signal(signal.SIGINT, interrupted)
print('scratch=' + str(scratch), flush=True)
env.update({
    prefix + '_EVENT_TRACE': str(c / 'traces' / (tag + '.events.tsv')),
    prefix + '_EVENT_TRACE_HEAP_VARS_ONLY': '1',
    prefix + '_EVENT_TRACE_CONFLICT_FROM': '13414',
    prefix + '_EVENT_TRACE_CONFLICT_TO': '13563',
    prefix + '_EVENT_TRACE_LIMIT': '10000',
    'SAT_EVENT_TRACE_HEAP_VARS': '109797,109796,109877,109815,109841',
})
env.pop(prefix + '_EVENT_TRACE_HEAP_VARS_ONLY', None)
for suffix in ('SKIP_HEAP', 'SKIP_WATCH', 'SKIP_DECISIONS', 'SKIP_RANDEC_DRAWS', 'SKIP_ANALYSIS_TICKS', 'IMPLY_ONLY'):
    env[prefix + '_EVENT_TRACE_' + suffix] = '1'
env[prefix + '_EVENT_TRACE_CONFLICT_FROM'] = '13563'
env['SAT_EVENT_TRACE_IMPLY_LITERALS'] = '109796,109815,109877,109841'
for key in list(env):
    if key.startswith('SAT_EVENT_TRACE_HEAP_SET_'):
        del env[key]
env.pop(prefix + '_CHOICE_STATE_TRACE', None)
env[prefix + '_MACRO_WATCH_TRACE'] = str(c / 'traces' / (tag + '.watch.tsv'))
env['SAT_MACRO_WATCH_EVENT'] = 'rephase_begin'
env['SAT_MACRO_WATCH_EXTERNAL'] = '-145'
env['SAT_MACRO_WATCH_CONFLICT'] = str(point)
r = dict(snapshot_conflict=point, scratch=str(scratch), arm=arm, slurm_job=os.environ['SLURM_JOB_ID'],
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
    with (c / 'logs' / (tag + '.out')).open('wb') as out:
        p = subprocess.run(command, stdout=out, stderr=subprocess.STDOUT, env=env)
    r['exit_code'] = p.returncode
    r['partial_certificate'] = dict(bytes=proof.stat().st_size, sha256=sha(proof)) if proof.exists() else None
    r['trace_sha256'] = {p.name: sha(p) for p in (c / 'traces').glob(tag + '.*')}
finally:
    shutil.rmtree(scratch)
    r['scratch_deleted'] = not scratch.exists()
    (c / 'results' / (tag + '.json')).write_text(json.dumps(r, indent=2) + '\n')
assert r['exit_code'] == 0, 'Unexpected solver exit: inspect before further performance work'
assert (c / 'traces' / (tag + '.prefix.tsv')).exists(), 'Requested stop was not observed'
print(arm, 'bounded stop and cleanup complete')

#!/usr/bin/env python3
"""Pin a separate exact-Release diagnostic with external RSS/smaps sampling."""
import datetime as dt
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import threading
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from proc_memory_task import monitor
c = root / 'benchmarks/runs/20260911-7298-proc-memory-rank31'
assert not c.exists(), 'preserve prepared or failed evidence'
prior = root / 'benchmarks/runs/20260911-7295-memory-syscalls-rank31'
m = json.loads((prior / 'manifest.json').read_text())
hashes = {}
def bind(p, expected=None):
    digest = sha(p)
    assert expected is None or digest == expected, p
    hashes[str(p.relative_to(root))] = digest
for name in ('manifest.json', 'summary.json', 'mapping-analysis-7297.json'):
    bind(prior / name)
for name in ('pins', 'logs', 'claims', 'provisional', 'results'):
    (c / name).mkdir(parents=True)
for name in ('runtime_task.py', 'modelcheck.py', 'drat-trim', 'checkmodel', 'strace', 'candidate', 'previous'):
    p = prior / 'pins' / name
    bind(p, m['pin_sha256']['pins/' + name])
    shutil.copy2(p, c / 'pins' / name)
for original, name in (('tools/bench/proc_memory_task.py', 'run.py'),
                       ('tools/bench/syscall_memory_task.py', 'syscall_memory_task.py')):
    p = root / original
    bind(p)
    shutil.copy2(p, c / 'pins' / name)
for name in ('test_proc_memory_task.py', 'test_syscall_memory_task.py'):
    bind(root / 'tools/bench' / name)
shutil.copy2(prior / 'matrix.tsv', c / 'matrix.tsv')
with (c / 'array.sbatch').open('x') as out:
    out.write((prior / 'array.sbatch').read_text().replace(str(prior), str(c)))
# Fixed allocator/proc fixture only, not a SAT-instance run. Exercise actual
# process identity, high-water capture and smaps under the pinned tracer.
trace = c / 'logs/preflight.memory-syscalls.txt'
output = c / 'logs/preflight.proc-memory.jsonl'
python = Path(sys.executable).resolve()
bind(python) if python.is_relative_to(root) else None
stop, errors = threading.Event(), []
def worker():
    try:
        monitor(trace, python, output, stop)
    except Exception as error:
        errors.append(repr(error))
observer = threading.Thread(target=worker)
observer.start()
fixture = 'import time; a=bytearray(16*1024*1024); time.sleep(.3); b=bytearray(32*1024*1024); time.sleep(.3); print(len(a)+len(b))'
command = [str(c / 'pins/strace'), '-f', '-qq', '-ttt', '-k', '-e',
           'trace=brk,mmap,mremap,munmap', '-o', str(trace), '--', str(python), '-c', fixture]
try:
    p = subprocess.run(command, text=True, capture_output=True, timeout=30)
finally:
    stop.set()
    observer.join()
assert not errors, errors
for name, text in (('preflight.stdout', p.stdout), ('preflight.stderr', p.stderr)):
    with (c / 'logs' / name).open('x') as out:
        out.write(text)
assert p.returncode == 0 and p.stdout.strip() == str(48*1024*1024), (p.returncode, p.stderr)
rows = [json.loads(line) for line in output.read_text().splitlines()]
samples = [r for r in rows if r['kind'] == 'sample']
assert len(samples) >= 2 and max(r['kib']['VmHWM'] for r in samples) >= 48*1024
assert any('smaps' in r.get('high_water_snapshot', {}) for r in samples)
assert len({r['starttime_ticks'] for r in samples}) == 1
assert os.sysconf('SC_PAGE_SIZE') == 4096
for p in (c / 'logs').iterdir():
    bind(p)
bind(Path(__file__))
bind(Path(__file__).with_name('validate-proc-memory-7298.py'))
for arm in m['release_records'].values():
    bind(root / arm['path'], arm['sha256'])
m.update(schema='tactix-external-proc-memory-v1',
    created_at=dt.datetime.now(dt.timezone.utc).isoformat(), evidence_sha256=hashes,
    preflight=dict(command=command, exit_code=0, samples=len(samples),
                   maximum_sampled_hwm_kib=max(r['kib']['VmHWM'] for r in samples)),
    hypothesis='7295 exact-Release RSS reproduced. Candidate retains a larger heap and reaches its traced virtual peak during later clause-arena growth. Observe solver-specific RSS/HWM and heap-region smaps to locate the actual resident excess.',
    acceptance='Exact whole own7267/7253 Release work/certificate fingerprints INCLUDING physical counters. Require observed GNUtime peaks and sampled solver VmHWM to reproduce562920/495156KiB before Release peak attribution. Keep timing out of acceptance; independent proofs continue asynchronously.',
    test_consideration='Eight new proc observer tests and nine mapping-ledger tests, including malformed units, PID/executable identity, exception shutdown, checker bypass, failed syscalls, remap/partial-map semantics and explicit unknown records. Actual fixed48MiB Python allocation/proc/smaps preflight; no real solver locally.',
    limitation='Sampling100ms brackets HWM increases but does not timestamp each allocation or make smaps atomic. Preserve raw before/after status and timestamps, gaps, incomplete stacks, and tracer versus solver RSS distinction. No preload, source instrumentation, allocator setting, score or previous campaign change.')
m['pin_sha256'] = {str(p.relative_to(c)): sha(p)
    for p in [*(c / 'pins').iterdir(), c / 'matrix.tsv', c / 'array.sbatch']}
with (c / 'manifest.json').open('x') as out:
    json.dump(m, out, indent=2)
for p in (c / 'pins').iterdir():
    p.chmod(p.stat().st_mode & ~0o222)
print(c)

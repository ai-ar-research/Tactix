#!/usr/bin/env python3
"""Pin exact Release solvers under an external allocator-system-call observer."""
import datetime as dt
import json
from pathlib import Path
import shutil
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
c = root / 'benchmarks/runs/20260911-7295-memory-syscalls-rank31'
assert not c.exists(), 'preserve prepared or failed evidence'
prior = root / 'benchmarks/runs/20260911-7286-memory-only-rank31'
m = json.loads((prior / 'manifest.json').read_text())
evidence = {}
def bind(p, expected=None):
    digest = sha(p)
    assert expected is None or digest == expected, p
    evidence[str(p.relative_to(root))] = digest
bind(prior / 'manifest.json')
bind(prior / 'summary.json')
bind(prior / 'comparison-7294.json')
for name in ('pins', 'logs', 'claims', 'provisional', 'results'):
    (c / name).mkdir(parents=True)
for name in ('runtime_task.py', 'modelcheck.py', 'drat-trim', 'checkmodel'):
    bind(prior / 'pins' / name, m['pin_sha256']['pins/' + name])
    shutil.copy2(prior / 'pins' / name, c / 'pins' / name)
shutil.copy2(prior / 'matrix.tsv', c / 'matrix.tsv')
wrapper = root / 'tools/bench/syscall_memory_task.py'
bind(wrapper)
bind(root / 'tools/bench/test_syscall_memory_task.py')
shutil.copy2(wrapper, c / 'pins/run.py')
tracer = Path('/usr/bin/strace')
shutil.copy2(tracer, c / 'pins/strace')
assert sha(tracer) == sha(c / 'pins/strace')
version = subprocess.check_output([str(c / 'pins/strace'), '-V'], text=True)
assert 'stack-trace=libdw' in version
fixture = root / 'build/pin-7267-dense-realloc-release/tests/test_realloc_words'
bind(fixture)
command = [str(c / 'pins/strace'), '-f', '-qq', '-ttt', '-k', '-e',
           'trace=brk,mmap,mremap,munmap', '--', str(fixture)]
p = subprocess.run(command, text=True, capture_output=True, timeout=60)
for label, text in (('preflight.stdout', p.stdout), ('preflight.stderr', p.stderr)):
    with (c / 'logs' / label).open('x') as out:
        out.write(text)
    bind(c / 'logs' / label)
assert p.returncode == 0 and 'operations=35815' in p.stdout
assert 'brk(' in p.stderr and str(fixture) + '(main+' in p.stderr
for arm, authority, pin in (
    ('candidate', '7271-dense-realloc', '7267-dense-realloc'),
    ('previous', '7253-occurrence-cutover', '7253-occurrence-cutover')):
    path = root / f'benchmarks/runs/20260911-{authority}-build/manifest.json'
    build = json.loads(path.read_text())
    bind(path)
    for rel, digest in build['files'].items():
        bind(root / build['source'] / rel, digest)
    binary = root / f'build/pin-{pin}-release/tactix'
    bind(binary, build['binaries'][str(binary.relative_to(root))])
    shutil.copy2(binary, c / 'pins' / arm)
    m['arms'][arm] = dict(path='pins/' + arm, sha256=sha(binary), arguments=[],
        source_commit=build['source_commit'], source_build=str(binary.parent.relative_to(root)))
    oracle = m['release_records'][arm]
    bind(root / oracle['path'], oracle['sha256'])
batch = (prior / 'array.sbatch').read_text().replace(str(prior), str(c))
batch = batch.replace('7286-memory-only-rank31', '7295-memory-syscalls-rank31')
with (c / 'array.sbatch').open('x') as out:
    out.write(batch)
bind(Path(__file__))
bind(Path(__file__).with_name('validate-memory-syscalls-7295.py'))
m.update(schema='tactix-external-memory-syscalls-v1',
    created_at=dt.datetime.now(dt.timezone.utc).isoformat(), evidence_sha256=evidence,
    tracer_version=version, preflight=dict(command=command, exit_code=p.returncode),
    hypothesis='Two source-instrumented memory observers reverse the Release RSS penalty. Trace allocator syscalls externally using exact7267/7253 binaries, no preload or in-process trace flags, to test reproduction and locate mapping/brk ownership.',
    acceptance='Exact whole own Release raw counters and certificates including physical counters; no excluded work fields. Report observed RSS versus562920/495156KiB and reject nonreproduction as Release attribution. Syscall tracing is diagnostic only; gaps in external unwind remain explicit. Independently check proofs and require final cleanup, but continue provisional work before proof completion.',
    test_consideration='Six wrapper tests cover both arms, preservation of outerperf/taskset, checker bypass, missing trace/failure, duplicate binary, existing output and negative task. Actual35815-operation fixture passes under pinned strace with allocator syscall and own main-frame evidence. No solver-source change.',
    limitation='Tracing changes scheduling/timing and can have overhead. GNUtime peak includes the tracer process too; compare actual peaks explicitly before attribution. Mapped bytes/brk extent are not resident bytes. No performance acceptance, observer success, memory fix or completed pending checks inferred.')
m['pin_sha256'] = {str(p.relative_to(c)): sha(p)
    for p in [*(c / 'pins').iterdir(), c / 'matrix.tsv', c / 'array.sbatch']}
with (c / 'manifest.json').open('x') as out:
    json.dump(m, out, indent=2)
for p in (c / 'pins').iterdir():
    p.chmod(p.stat().st_mode & ~0o222)
print(c)

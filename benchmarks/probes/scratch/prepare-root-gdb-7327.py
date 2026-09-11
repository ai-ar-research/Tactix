#!/usr/bin/env python3
"""Pin two exact ordinary binaries, only one externally debugged."""
import datetime as dt
import json
from pathlib import Path
import shutil
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root/'tools/bench'))
from fixed_runtime import sha
c = root/'benchmarks/runs/20260911-7327-rank25-root-gdb'
assert not c.exists(), 'prepared campaign frozen'
prior = root/'benchmarks/runs/20260911-7323-rank25-root-census'
m = json.loads((prior/'manifest.json').read_text())
fixture = root/'benchmarks/runs/20260911-7326-root-gdb-fixture/summary.json'
f = json.loads(fixture.read_text())
assert f['status'] == 'PASS'
evidence = {}
def bind(p, expected=None):
    digest = sha(p)
    assert expected is None or digest == expected, p
    evidence[str(p.relative_to(root))] = digest
bind(fixture); bind(prior/'manifest.json'); bind(prior/'summary.json')
for rel, digest in f['hashes'].items():
    bind(root/rel, digest)
for name in ('pins', 'logs', 'claims', 'provisional', 'results'):
    (c/name).mkdir(parents=True)
for name in ('runtime_task.py', 'modelcheck.py', 'drat-trim', 'checkmodel'):
    bind(prior/'pins'/name, m['pin_sha256']['pins/'+name])
    shutil.copy2(prior/'pins'/name, c/'pins'/name)
gdb = Path(f['debugger']['path'])
assert sha(gdb) == f['debugger']['sha256']
shutil.copy2(gdb, c/'pins/gdb')
for name in ('root_memory.py', 'root_gdb.py', 'root_gdb_task.py'):
    shutil.copy2(root/'tools/bench'/name, c/'pins'/name)
shutil.copy2(Path(__file__).with_name('run-root-gdb-7327.py'), c/'pins/run.py')
shutil.copy2(prior/'matrix.tsv', c/'matrix.tsv')
control = prior/'pins/previous'
assert sha(control) == 'fc1a4ca2634c4312b22dc844a1fd7ce5897bae34481f74b362a1220fa1519c54'
oracle = m['release_records']['previous']
for arm in ('candidate', 'previous'):
    shutil.copy2(control, c/'pins'/arm)
    m['arms'][arm] = dict(m['arms']['previous'], path='pins/'+arm)
    m['release_records'][arm] = dict(oracle)
config = dict(binary_sha256=sha(control), layout=f['layout'],
    breakpoint_address=f['ordinary_address'], instruction_hex=f['instruction_hex'],
    minimum_new_capacity_bytes=474611712, expected_exit_codes=[10])
with (c/'pins/root-gdb-config.json').open('x') as out:
    json.dump(config, out, indent=2)
with (c/'array.sbatch').open('x') as out:
    out.write((prior/'array.sbatch').read_text().replace(str(prior), str(c)).replace(
        '7323-rank25-root-census', '7327-rank25-root-gdb'))
for name in ('prepare-root-gdb-7327.py', 'run-root-gdb-7327.py', 'validate-root-gdb-7327.py'):
    bind(Path(__file__).with_name(name))
m.pop('tracer_version', None)
m.update(schema='tactix-rank25-root-gdb-v1', created_at=dt.datetime.now(dt.timezone.utc).isoformat(),
    evidence_sha256=evidence,
    hypothesis='Ordinary7302 retains millions of root-fixed slots at witness-growth peak; read live populations and old/new reconstruction buffers before old free.',
    acceptance=f['gate'],
    test_consideration='Nine decoder/wrapper tests and compiled8-variable witness fixture with exact root/nonroot/retired counts, stats and64->128B old/new growth; no solver rebuild.',
    limitation='One read-only GDB-host snapshot; no strace or in-process census. Source/layout/ordinary binary pinned. Diagnostic wall/perf include debugger work and are NOT timing evidence. Existing campaigns unchanged.',
    preflight_authority=dict(path=str(fixture.relative_to(root)), sha256=sha(fixture)),
    debugger_version=subprocess.check_output([str(gdb), '--version'], text=True))
m['pin_sha256'] = {str(p.relative_to(c)):sha(p) for p in [*(c/'pins').iterdir(), c/'matrix.tsv', c/'array.sbatch']}
with (c/'manifest.json').open('x') as out:
    json.dump(m, out, indent=2)
for p in (c/'pins').iterdir():
    p.chmod(p.stat().st_mode & ~0o222)
print(c)

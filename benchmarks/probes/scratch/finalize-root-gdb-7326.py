#!/usr/bin/env python3
"""Seal already-passing fixture evidence after the manifest-path type error."""
import json
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root/'tools/bench'))
from fixed_runtime import sha
c = root/'benchmarks/runs/20260911-7326-root-gdb-fixture'
target = c/'summary.json'
assert not target.exists() and (c/'manifest.json').stat().st_size == 0
config = json.loads((c/'root-gdb-config.json').read_text())
assert sha(c/'fixture') == config['binary_sha256']
rows = [json.loads(line) for line in (c/'fixture.root-gdb.jsonl').read_text().splitlines()]
assert [r['kind'] for r in rows] == ['start', 'snapshot', 'done']
assert rows[-1] == dict(kind='done', captures=1, errors=[], exits=[0])
assert rows[1]['state']['populations'] == dict(vars=8, fixed=2, fixed_active=2,
    retired=1, unassigned=4, nonroot_active=1)
assert (rows[1]['state']['conflicts'], rows[1]['state']['decisions']) == (17, 29)
assert rows[1]['old_reconstruction']['capacity_bytes'] == 64
assert rows[1]['new_reconstruction_capacity_bytes'] == 128
for name in ('plain-fixture', 'gdb-fixture'):
    assert 'fixed witness fixture PASS' in (c/'logs'/f'{name}.log').read_text()
hashes = {}
paths = [p for p in c.rglob('*') if p.is_file() and '__pycache__' not in p.parts]
paths += [Path(__file__), Path(__file__).with_name('build-root-gdb-7326.py'),
          Path(__file__).with_name('root-layout-7326.cpp')]
paths += list((root/'build/pin-7322-root-census-control/CMakeFiles/tactix_lib.dir').rglob('*.o'))
paths += list((root/'.claude/worktrees/runtime-clause-realloc-7301/src').rglob('*.hpp'))
paths += [root/'tools/bench'/name for name in ('root_memory.py', 'test_root_memory.py',
          'root_gdb.py', 'root_gdb_task.py', 'test_root_gdb_task.py')]
for p in paths:
    hashes[str(p.relative_to(root))] = sha(p)
with target.open('x') as out:
    json.dump(dict(status='PASS', hashes=hashes, layout=config['layout'], snapshot=rows[1],
        debugger=dict(path='/usr/bin/gdb', sha256=sha(Path('/usr/bin/gdb'))),
        ordinary_address=0x40ed1f, instruction_hex='4889ef',
        preparation_error='Build, plain fixture and GDB fixture passed; final manifest construction passed a str instead of Path to sha. Preserve original helper/logs and empty manifest; this summary seals the existing passing evidence without rebuilding or rerunning.',
        gate='One snapshot before old witness free, exact ordinary work/certificate/RSS; independent model and zero residue.',
        limitation='External debugger only; no elapsed-time inference or solver source change.'), out, indent=2)
print(target)

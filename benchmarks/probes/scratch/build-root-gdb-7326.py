#!/usr/bin/env python3
"""Build a fixed layout/witness fixture; never rebuild the ordinary solver."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root/'tools/bench'))
from fixed_runtime import sha
source = root/'.claude/worktrees/runtime-clause-realloc-7301'
objects = root/'build/pin-7322-root-census-control/CMakeFiles/tactix_lib.dir'
c = root/'benchmarks/runs/20260911-7326-root-gdb-fixture'
assert not c.exists(), 'fixture build already exists; preserve its evidence'
c.mkdir()
(c/'logs').mkdir()
binary = c/'fixture'
fixture = Path(__file__).with_name('root-layout-7326.cpp')
hashes, commands = {}, {}
def bind(p):
    hashes[str(p.relative_to(root))] = sha(p)
def run(name, command, **kwargs):
    result = subprocess.run(command, text=True, capture_output=True, **kwargs)
    with (c/'logs'/f'{name}.log').open('x') as out:
        out.write(result.stdout+result.stderr)
    commands[name] = dict(command=command, exit_code=result.returncode)
    assert result.returncode == 0, (name, result.stdout, result.stderr)
    return result.stdout
objs = sorted(objects.rglob('*.o'))
assert len(objs) > 30
for p in [fixture, Path(__file__), *objs, *source.joinpath('src').rglob('*.hpp')]:
    bind(p)
run('build', ['/cm/local/apps/gcc/13.1.0/bin/g++', '-std=c++20', '-O3', '-DNDEBUG',
    '-fno-exceptions', '-fno-rtti', '-no-pie', '-I'+str(source/'src'), '-I'+str(source/'include'),
    str(fixture), *map(str, objs), '-o', str(binary)])
layout = json.loads(run('layout', [str(binary)]))
assert layout['kernel_bytes'] == 7872 and layout['vectors']['reconstruction'] == [680, 4]
run('plain-fixture', [str(binary), 'fixture'])
symbol = '_ZN6tactix19kernel_push_witnessERNS_6KernelEjPKjj'
symbols = run('symbols', ['nm', '--defined-only', str(binary)])
base = int(next(line.split()[0] for line in symbols.splitlines() if line.endswith(' '+symbol)), 16)
address = base+0x17f
run('assembly', ['objdump', '-d', '--start-address='+hex(base), '--stop-address='+hex(base+0x1a0), str(binary)])
config = dict(binary_sha256=sha(binary), layout=layout, breakpoint_address=address,
              minimum_new_capacity_bytes=128, instruction_hex='4889ef', expected_exit_codes=[0])
with (c/'root-gdb-config.json').open('x') as out:
    json.dump(config, out, indent=2)
for name in ('root_gdb.py', 'root_memory.py'):
    shutil.copy2(root/'tools/bench'/name, c/name)
env = dict(os.environ, TACTIX_GDB_CONFIG=str(c/'root-gdb-config.json'),
           TACTIX_GDB_OUTPUT=str(c/'fixture.root-gdb.jsonl'))
command = ['/usr/bin/gdb', '-q', '-nx', '-batch', '--return-child-result']
for setting in ('pagination off', 'confirm off', 'auto-load off', 'debuginfod enabled off',
                'disable-randomization off', 'startup-with-shell off'):
    command += ['-ex', 'set '+setting]
command += ['-ex', 'python exec(compile(open('+repr(str(c/'root_gdb.py'))+
            ').read(), "root_gdb.py", "exec"))', '--args', str(binary), 'fixture']
run('gdb-fixture', command, env=env)
rows = [json.loads(line) for line in (c/'fixture.root-gdb.jsonl').read_text().splitlines()]
assert [r['kind'] for r in rows] == ['start', 'snapshot', 'done']
assert rows[-1] == dict(kind='done', captures=1, errors=[], exits=[0])
state = rows[1]['state']
assert state['populations'] == dict(vars=8, fixed=2, fixed_active=2, retired=1,
                                    unassigned=4, nonroot_active=1)
assert (state['conflicts'], state['decisions']) == (17, 29)
assert rows[1]['old_reconstruction']['capacity_bytes'] == 64
assert rows[1]['new_reconstruction_capacity_bytes'] == 128
for p in c.rglob('*'):
    if p.is_file() and '__pycache__' not in p.parts:
        bind(p)
for name in ('root_memory.py', 'test_root_memory.py', 'root_gdb.py',
             'root_gdb_task.py', 'test_root_gdb_task.py'):
    bind(root/'tools/bench'/name)
with (c/'manifest.json').open('x') as out:
    json.dump(dict(status='PASS', hashes=hashes, commands=commands, layout=layout,
                  snapshot=rows[1], debugger=dict(path='/usr/bin/gdb', sha256=sha('/usr/bin/gdb')),
                  ordinary_address=0x40ed1f, instruction_hex='4889ef',
                  gate='One exact snapshot before old witness free, ordinary binary work/certificate/RSS reproduction; independent model and zero residue.',
                  limitation='Read-only debugger-host diagnostic, not elapsed-time evidence. Fixed fixture is synthetic, not an additional benchmark.'), out, indent=2)
print(c)

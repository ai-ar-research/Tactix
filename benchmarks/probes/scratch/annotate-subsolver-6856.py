#!/usr/bin/env python3
"""Inspect existing checked Rank39 samples; never execute a solver."""
from pathlib import Path
import json
import lzma
import re
import shutil
import subprocess
import sys
import tempfile
root = Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
from perf_annotation import parse_periods
subprocess.run([sys.executable,str(root/'tools/slurm/session_deadline.py'),'--deadline','2026-09-10T06:00:00-04:00'],check=True)
parent = root/'benchmarks/runs/20260910-6815-inprocessing-release-profiles'
summary = parent/'solver-summary-6817.json'
s = json.loads(summary.read_text())
for rel,digest in s['hashes'].items(): assert sha(root/rel) == digest,rel
profile = s['arms']['39/candidate']['profile']
compressed = parent/'profiles/002.perf.data.xz'
assert sha(compressed) == profile['compressed_sha256']
binary = parent/'pins/candidate'
source = root/'.claude/worktrees/runtime-reduce-watermark-6730/src/engine/subsolver.cpp'
assert sha(binary) == '7b0a7666b28644422d6cc4b3a648a677b6745bd8a7ffb9c27c6d3e5660725132'
c = root/'benchmarks/runs/20260910-6856-subsolver-propagation-annotation'
assert not c.exists(); c.mkdir()
symbol = 'tactix::SubSolver::propagate()'
command = ['objdump','-dC','--no-show-raw-insn','--disassemble='+symbol,str(binary)]
assembly = subprocess.check_output(command,text=True)
(c/'objdump.txt').write_text(assembly)
addresses = [int(m[1],16) for line in assembly.splitlines() if (m := re.match(r'^\s*([0-9a-f]+):\s+\S',line))]
assert addresses and addresses[0] == 0x4e8c80
with tempfile.TemporaryDirectory(prefix='annotation-',dir=c) as directory:
    data = Path(directory)/'profile.data'
    with lzma.open(compressed,'rb') as src, data.open('xb') as dst: shutil.copyfileobj(src,dst)
    assert sha(data) == profile['sha256']
    removed = data.stat().st_size
    invocation = ['perf','annotate','--stdio','--stdio-color','never','--no-source','-i',str(data),'--symbol',symbol,'--show-total-period']
    r = subprocess.run(invocation,cwd=root,capture_output=True,text=True)
    (c/'annotation.txt').write_text(r.stdout); (c/'annotation.stderr').write_text(r.stderr)
    assert r.returncode == 0 and 'no samples' not in r.stderr.lower(),r.stderr
    a = parse_periods(r.stdout,symbol)
    assert [v.address for v in a.instructions] == addresses
assert not Path(directory).exists()
hashes = {str(p.relative_to(root)):sha(p) for p in (Path(__file__),summary,compressed,binary,source,
    root/'tools/bench/perf_annotation.py',root/'tools/bench/test_perf_annotation.py',c/'objdump.txt',c/'annotation.txt',c/'annotation.stderr')}
rows = [dict(address=hex(v.address),offset=v.address-addresses[0],period=v.period,assembly=v.assembly) for v in a.instructions]
out = dict(hashes=hashes,command=invocation,symbol=symbol,samples=a.samples,total_period=a.total_period,
    rows=rows,highest_sampled_instructions=sorted(rows,key=lambda v:v['period'],reverse=True)[:25],
    temporary_expansions_deleted_bytes=removed,
    test_consideration='Existing strict perf_annotation tests cover sampled-period framing/order and exact range boundaries. Every actual instruction address equals the independent objdump inventory. No parser or solver edit.',
    limitation='Existing checked current6730 Rank39 samples only. Instruction skid/sampling apply; samples select source regions, not removable-cost ceilings, native unit prices or predicted gains. Raw annotations retained locally/hash-bound, never rewritten to satisfy whitespace checks.')
with (c/'summary.json').open('x') as stream:
    json.dump(out,stream,indent=2); stream.write('\n')
print(json.dumps(dict(samples=a.samples,total_period=a.total_period,removed_bytes=removed,top=out['highest_sampled_instructions'][:12]),indent=2))

#!/usr/bin/env python3
"""Freeze tested metadata-preserving source and independent Release builds."""
import difflib
import hashlib
import json
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260907-5426-watch-truncate'
source = root / '.claude/worktrees/runtime-watch-truncate-5426'
parent = root / '.claude/worktrees/runtime-rephase-root-5417'
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
for name in ('release', 'bc'):
    assert '100% tests passed out of 78' in (c / 'logs' / f'ctest-{name}.log').read_text()
fixture_runs = {}
for label in ('old', 'new'):
    binary = root / f'benchmarks/probes/scratch/watch-metadata-{label}-5426'
    result = subprocess.run([binary], capture_output=True, text=True, check=True)
    assert '392 exact old/new preservation cases passed' in result.stdout
    fixture_runs[label] = dict(binary_sha256=sha(binary), output=result.stdout)
files = ['src/kernel/watch.hpp', 'tests/test_watch_metadata.cpp']
def patch(before, paths):
    result = ''
    for f in paths:
        a, b = before / f, source / f
        old = a.read_text().splitlines(True) if a.exists() else []
        result += ''.join(difflib.unified_diff(old, b.read_text().splitlines(True), fromfile='a/' + f if a.exists() else '/dev/null', tofile='b/' + f))
    return result
for name, body in [('repair.patch', patch(parent, files)), ('cumulative.patch', patch(root, [str(p.relative_to(root)) for p in sorted((root / 'src').rglob('*')) if p.is_file()]))]:
    assert not (c / name).exists()
    (c / name).write_text(body)
builds = ['build/pin-5426-watch-truncate-release/tactix', 'build/pin-5427-watch-truncate-bc/tactix']
assert sha(root / builds[0]) == sha(root / builds[1])
assembly = {}
for name, binary in [('old', root / 'build/pin-5419-rephase-root-release/tactix'), ('new', root / builds[0])]:
    # Do not combine --demangle with a mangled --disassemble selector.
    output = subprocess.run(['objdump', '-d', '--no-show-raw-insn', '--disassemble=_ZN6tactix16kernel_propagateERNS_6KernelEj', str(binary)], capture_output=True, text=True, check=True).stdout
    assert 'kernel_propagate' in output and len(output.splitlines()) > 100
    path = c / (name + '-propagate.asm')
    path.write_text(output)
    assembly[path.name] = sha(path)
m = dict(purpose='Remove unchanged packed watch metadata loads/stores on large-only append/truncate; preserve every physical and semantic state field.',
    source=str(source.relative_to(root)), parent=str(parent.relative_to(root)), production='c2a95a6c79c21a76516cf556b6538e76da369c34',
    hypothesis='Source and baseline assembly show a redundant packed-metadata load/store on each full watch scan and unnecessary metadata reconstruction on delayed append. Eliminate these operations without altering traversal, representation layout or scheduling; measure rather than assume a timing win.',
    invariant='Two-word changes preserve size parity, binary population, and the conservative order bit. Arena grow only changes placement/capacity. Truncate asserts even removal and sufficient retained binary words. No guard, proof, N1 or scheduling change.',
    fixture='392 identical actual-object cases on old and new headers: odd/even binary populations, large/binary interleaving, every large removal count, no-op/empty cases, repeated arena growth, exact encoded metadata, entries, sizes, holes, offsets and capacities.',
    fixture_runs=fixture_runs, status='UNFUZZED isolated equivalent implementation; no runtime acceptance or production promotion',
    files={f:sha(source / f) for f in files}, binaries={p:sha(root / p) for p in builds},
    patches={p.name:sha(p) for p in c.glob('*.patch')}, assembly=assembly,
    logs={p.name:sha(p) for p in (c / 'logs').iterdir()})
assert not (c / 'manifest.json').exists()
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
print('PASS: 78/78 twice, equal Release hashes, 392 old/new fixture cases; source frozen')

#!/usr/bin/env python3
"""Extract saved profile callstacks offline; never execute either solver."""
import json
import lzma
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from checked_comparison import classify
from fixed_runtime import sha
c = root / 'benchmarks/runs/20260910-7081-rank35-callstack-cost'
assert not c.exists()
c.mkdir()
hashes = {}; records = {}; deleted = 0
def bind(p, expected=None):
    value = sha(p)
    assert expected is None or value == expected, p
    hashes[str(p.relative_to(root))] = value
for arm, directory, task in (
    ('candidate', '20260910-7060-rank35-residual-profile', 0),
    ('kissat', '20260910-7002-largest-gap-release-profiles', 3)):
    parent = root / 'benchmarks/runs' / directory
    p = parent / f'results/{task:03d}.json'; bind(p)
    record = json.loads(p.read_text()); state = classify(record, True)
    assert state['status'] == 'checked_UNSAT' and state['checked'] and state['clean']
    assert record['rank'] == 35 and record['arm'] == arm
    profile = record['solve']['profile']
    binary = parent / 'pins' / arm; bind(binary, record['binary_sha256'])
    compressed = parent / f'profiles/{task:03d}.perf.data.xz'
    bind(compressed, profile['compressed_sha256'])
    for name, info in profile['reports'].items():
        bind(parent / f'profiles/{task:03d}-{name}.txt', info['sha256'])
    directory = Path(tempfile.mkdtemp(prefix='expanded-', dir=c))
    data = directory / 'profile.data'
    with lzma.open(compressed, 'rb') as src, data.open('xb') as dst:
        shutil.copyfileobj(src, dst)
    assert sha(data) == profile['sha256'] and data.stat().st_size == profile['bytes']
    command = ['perf', 'script', '-i', str(data), '-F', 'period,ip,sym,dso']
    output = c / (arm + '.samples.txt.xz')
    stderr = c / (arm + '.stderr')
    sample_lines = []; lines = 0
    with stderr.open('xb') as err, lzma.open(output, 'xb') as dst:
        process = subprocess.Popen(command, cwd=root, stdout=subprocess.PIPE, stderr=err)
        for line in process.stdout:
            dst.write(line); lines += 1
            if len(sample_lines) < 16:
                sample_lines.append(line.decode().rstrip())
        process.stdout.close()
        assert process.wait() == 0, stderr.read_text()
    assert lines > 1000 and not stderr.read_text().strip(), stderr.read_text()
    bind(output); bind(stderr)
    records[arm] = dict(record=str(p.relative_to(root)), state=state, command=command,
                        lines=lines, raw_profile_bytes=data.stat().st_size,
                        sample_stream=str(output.relative_to(root)))
    print(arm, 'lines', lines, 'compressed bytes', output.stat().st_size, flush=True)
    for line in sample_lines: print(line[:250], flush=True)
    # Exactly this helper's new expansion, no job references or inbound
    # authority. The verified original compressed profile remains untouched.
    assert data.parent == directory and directory.parent == c
    assert list(directory.iterdir()) == [data]
    assert sha(data) == profile['sha256']
    deleted += data.stat().st_size
    data.unlink(); directory.rmdir()
symbol = 'tactix::DefaultPolicy::solve(tactix::Kernel&, int volatile*)'
binary = root / 'benchmarks/runs/20260910-7060-rank35-residual-profile/pins/candidate'
assembly = subprocess.check_output(['objdump', '-dC', '--no-show-raw-insn',
                                   '--disassemble=' + symbol, str(binary)], text=True)
p = c / 'solve-disassembly.txt'
with p.open('x') as stream: stream.write(assembly)
bind(p); bind(Path(__file__))
for rel in ('src/policies/default_policy.cpp', 'src/policies/default_policy.hpp'):
    bind(root / '.claude/worktrees/runtime-collect-retry-7030' / rel)
bind(root / 'benchmarks/reports/2026-09-10-rank35-cost-scopes-7079.json')
out = dict(hashes=hashes, records=records, temporary_expansions_deleted_bytes=deleted,
           decision='Use caller PCs to distinguish current initial Probe, search Probe and Eliminate without new solver executions. Native has distinct entry symbols. Parsing, conservation and exact call-site validation still required before cost interpretation.',
           limitation='Offline extraction only. Zero stderr and exact input hashes do not establish correct attribution or semantic work equality. The next validator owns framing, sample-period conservation and unknown call stacks.')
with (c / 'extraction-7081.json').open('x') as stream:
    json.dump(out, stream, indent=2); stream.write('\n')
print(c / 'extraction-7081.json', flush=True)

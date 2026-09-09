#!/usr/bin/env python3
"""Annotate existing checked Rank32 profiles; never execute a solver."""
import json
import lzma
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha

parent = root / 'benchmarks/runs/20260909-6372-rank32-release-profile'
target = root / 'benchmarks/runs/20260909-6530-rank32-retention-annotation'
assert not target.exists()
manifest = json.loads((parent / 'manifest.json').read_text())
summary = json.loads((parent / 'solver-summary-6378.json').read_text())
source = root / '.claude/worktrees/runtime-compact-stamp-6357'
current = root / '.claude/worktrees/runtime-activation-stamp-6424'
source_hashes = {}
for rel in ('src/kernel/kernel.cpp', 'src/kernel/watch.hpp'):
    assert sha(source / rel) == sha(current / rel), rel
    source_hashes[rel] = sha(source / rel)
target.mkdir()
symbols = {
    'candidate': {
        'rebuild': 'tactix::kernel_rebuild_large_watches(tactix::Kernel&, bool)',
        'collect': 'tactix::kernel_collect_garbage(tactix::Kernel&, unsigned int, bool, bool)',
    },
    'kissat': {
        'rebuild': 'kissat_watch_large_clauses',
        'flush': 'kissat_flush_large_watches',
    },
}
outputs = {}
authorities = {'manifest.json': sha(parent / 'manifest.json'),
               'solver-summary-6378.json': sha(parent / 'solver-summary-6378.json')}
deleted_bytes = 0
for task, arm in enumerate(('candidate', 'kissat')):
    p = parent / f'results/{task:03}.json'
    result = json.loads(p.read_text())
    assert result['valid'] and result['proof_check']['result'] == 'verified'
    assert result['artifact_deleted'] and not result['cleanup_pending']
    authorities[str(p.relative_to(parent))] = sha(p)
    profile = summary['arms'][arm]['profile']
    compressed = parent / f'profiles/{task:03}.perf.data.xz'
    assert sha(compressed) == profile['compressed_sha256']
    assert sha(parent / 'pins' / arm) == manifest['arms'][arm]['sha256']
    authorities[str(compressed.relative_to(parent))] = sha(compressed)
    authorities['pins/' + arm] = sha(parent / 'pins' / arm)
    with tempfile.TemporaryDirectory(prefix=arm + '-', dir=target) as directory:
        data = Path(directory) / 'profile.data'
        with lzma.open(compressed, 'rb') as src, data.open('xb') as dst:
            shutil.copyfileobj(src, dst)
        assert sha(data) == profile['sha256']
        deleted_bytes += data.stat().st_size
        for label, symbol in symbols[arm].items():
            for mode, options in (('periods', ['--show-total-period']),
                                  ('percent', ['--percent-type', 'local-period'])):
                name = arm + '-' + label + '-' + mode
                command = ['perf', 'annotate', '--stdio', '--stdio-color', 'never',
                           '--no-source', '-i', str(data), '--symbol', symbol, *options]
                run = subprocess.run(command, cwd=root, capture_output=True, text=True)
                (target / (name + '.txt')).write_text(run.stdout)
                (target / (name + '.stderr')).write_text(run.stderr)
                assert run.returncode == 0 and run.stdout.strip(), (name, run.stderr)
                assert 'no samples' not in run.stderr.lower(), (name, run.stderr)
                outputs[name] = dict(command=command, exit=run.returncode,
                    stdout_sha256=sha(target / (name + '.txt')),
                    stderr_sha256=sha(target / (name + '.stderr')))
                print(name, 'PASS', flush=True)
    assert not Path(directory).exists()
with (target / 'manifest.json').open('x') as out:
    json.dump(dict(parent=str(parent.relative_to(root)), authorities=authorities,
        source_kernel_watch_exact_current=source_hashes, outputs=outputs,
        temporary_expansions_deleted_bytes=deleted_bytes,
        scope='Offline attribution of already checked profiles; no new solver/profile/correctness or runtime-gain claim.',
        test_consideration='Existing annotate helper pattern, exact pin/profile/source hashes and nonempty successful symbol outputs. No solver/parser implementation changed; new attribution parser will need focused tests if introduced.'), out, indent=2)
    out.write('\n')
print('PASS: eight annotations, exact inputs, owned temporary expansions deleted')

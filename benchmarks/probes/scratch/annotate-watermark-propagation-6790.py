#!/usr/bin/env python3
"""Attribute existing replay-validated samples; never run or change a solver."""
import json
import lzma
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root/'tools/bench'))
from fixed_runtime import sha
from perf_annotation import parse_periods
subprocess.run([sys.executable, str(root/'tools/slurm/session_deadline.py'),
    '--deadline', '2026-09-10T06:00:00-04:00'], check=True)
parent = root/'benchmarks/runs/20260909-6787-rank38-watermark-profile'
target = root/'benchmarks/runs/20260909-6790-watermark-propagation-annotation'
assert not target.exists()
summary_path = parent/'solver-summary-6789.json'
s = json.loads(summary_path.read_text())
for rel, digest in s['hashes'].items():
    assert sha(root/rel) == digest, rel
static_path = root/'benchmarks/runs/20260909-6777-watermark-propagate-code/summary.json'
static = json.loads(static_path.read_text())
for rel, digest in static['hashes'].items():
    assert sha(root/rel) == digest, rel
target.mkdir()
hashes = {str(p.relative_to(root)):sha(p) for p in
    (summary_path, static_path, Path(__file__), root/'tools/bench/perf_annotation.py', root/'tools/bench/test_perf_annotation.py')}
symbol = 'tactix::kernel_propagate(tactix::Kernel&, unsigned int)'
arms = {}
deleted_bytes = 0
for task, arm in enumerate(('candidate', 'previous')):
    profile = s['arms'][arm]['profile']
    compressed = parent/f'profiles/{task:03d}.perf.data.xz'
    assert sha(compressed) == profile['compressed_sha256']
    with tempfile.TemporaryDirectory(prefix=arm+'-', dir=target) as directory:
        data = Path(directory)/'profile.data'
        with lzma.open(compressed, 'rb') as src, data.open('xb') as dst:
            shutil.copyfileobj(src, dst)
        assert sha(data) == profile['sha256']
        deleted_bytes += data.stat().st_size
        command = ['perf', 'annotate', '--stdio', '--stdio-color', 'never', '--no-source',
            '-i', str(data), '--symbol', symbol, '--show-total-period']
        run = subprocess.run(command, cwd=root, capture_output=True, text=True)
        for suffix, body in (('txt', run.stdout), ('stderr', run.stderr)):
            p = target/f'{arm}.{suffix}'
            p.write_text(body)
            hashes[str(p.relative_to(root))] = sha(p)
        assert run.returncode == 0 and run.stdout.strip() and 'no samples' not in run.stderr.lower(), run.stderr
        annotation = parse_periods(run.stdout, symbol)
        code = static['records']['watermark' if arm == 'candidate' else 'before']
        rows = [dict(offset=r.address-code['address'], period=r.period, assembly=r.assembly)
                for r in annotation.instructions]
        assert [r['offset'] for r in rows] == [r['offset'] for r in code['instruction_rows']]
        arms[arm] = dict(command=command, samples=annotation.samples, total_period=annotation.total_period,
            address=code['address_hex'], rows=rows)
        print(arm, annotation.samples, annotation.total_period, flush=True)
    assert not Path(directory).exists()
assert [r['offset'] for r in arms['candidate']['rows']] == [r['offset'] for r in arms['previous']['rows']]
deltas = [dict(offset=a['offset'], candidate=a, previous=b, delta=a['period']-b['period'])
          for a,b in zip(arms['candidate']['rows'], arms['previous']['rows'])]
out = dict(hashes=hashes, arms=arms, largest_positive_deltas=sorted(deltas, key=lambda r:r['delta'], reverse=True)[:20],
    total_period_ratio=arms['candidate']['total_period']/arms['previous']['total_period'],
    temporary_expansions_deleted_bytes=deleted_bytes,
    test_consideration='Existing seven strict perf-annotation tests cover exact integer periods, headers, instruction identity/order and valid boundaries. Every actual instruction offset is checked against independent6777 objdump inventory. No parser or solver change.',
    limitation='Sampled cycles include skid and statistical noise, not exact instruction costs. Corresponding offsets localize candidate regions but do not establish cache/front-end causality. No live checker scratch touched; only this helper-owned temporary perf expansions are removed.')
with (target/'summary.json').open('x') as stream:
    json.dump(out, stream, indent=2); stream.write('\n')
print(json.dumps(dict(ratio=out['total_period_ratio'], top=out['largest_positive_deltas'][:8]), indent=2))

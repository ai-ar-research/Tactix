#!/usr/bin/env python3
"""Bound current6730 against native, not a cheaper historical Tactix branch."""
import json
from pathlib import Path
import shutil
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha

c = root / 'benchmarks/runs/20260910-7011-rank35-current-native-prefix'
assert not c.exists()
old = root / 'benchmarks/runs/20260909-6376-rank35-stamp-prefix'
native = root / 'benchmarks/runs/20260907-5433-rank35-inprocess-prefix'
current = root / 'benchmarks/runs/20260909-6730-reduce-watermark-build/manifest.json'
dependencies = {}
def bind(path, expected=None):
    digest = sha(path)
    assert expected is None or digest == expected, path
    dependencies[str(path.relative_to(root))] = digest

for directory in (old, native):
    bind(directory / 'manifest.json')
    manifest = json.loads((directory / 'manifest.json').read_text())
    for rel, digest in manifest['hashes'].items():
        bind(directory / rel, digest)
    bind(directory / 'summary.json')
bind(current)
build = json.loads(current.read_text())
for rel, digest in build['files'].items():
    bind(root / build['source'] / rel, digest)
candidate = root / 'build/pin-6730-reduce-watermark-trace/tactix'
bind(candidate, build['binaries'][str(candidate.relative_to(root))])
om = json.loads((old / 'manifest.json').read_text())
bind(root / om['input'], om['input_sha256_compressed'])
for rel in ('benchmarks/runs/20260910-7006-transitive-largest-gap-ablation/solver-summary-7008.json',
            'tools/bench/decision_prefix.py', 'tools/bench/test_decision_prefix.py'):
    bind(root / rel)
bind(Path(__file__))
for folder in ('pins', 'logs', 'traces', 'results'):
    (c / folder).mkdir(parents=True)
shutil.copy2(candidate, c / 'pins/candidate')
shutil.copy2(native / 'pins/kissat', c / 'pins/kissat')
runner = (old / 'run.py').read_text().replace("('candidate', 'previous')", "('candidate', 'kissat')").replace('tactix-6376-', 'tactix-7011-')
assert "('candidate', 'kissat')" in runner
(c / 'run.py').write_text(runner)
batch = (old / 'array.sbatch').read_text().replace(str(old), str(c)).replace('6376-rank35-stamp-prefix', '7011-rank35-current-native-prefix')
assert '--array=0-1\n' in batch and '--nodelist' not in batch
(c / 'array.sbatch').write_text(batch)
m = dict(rank=35, input=om['input'], input_sha256_compressed=om['input_sha256_compressed'],
         stop_conflict=1000000, stream_from=100000, common_decisions=40636523,
         prior_common_hash='6a3ebeb3a46d7ad5', resource_contract=om['resource_contract'],
         session_authority='Explicit user continue after Sep10 06:00EDT; new cutoff requested and pending.',
         purpose='Locate current6730 first actual native decision/scheduling difference after the historical100K closure.7008 demonstrates material TransitiveReduce downstream effects but not the first native discrepancy.',
         hypothesis='A current/native divergence through1M may expose an earlier causal worse-work boundary than the old current/6305 late Reorder witness. Equal decisions do not prove formula identity.',
         validation='Classify clean stopped diagnostics before interpretation; require observed1M stops, complete bound trace hashes, prefix framing and accounting, and scratch deletion. Test both inherited100K common hashes independently. If either changes, localize earlier rather than assuming inherited closure. Cached native and6376 streams end below100K: verify zero overlap, suffix count and all suffix rows before claiming native agreement.',
         decision='If a native choice split is observed, align preceding portable macro boundaries and investigate only a materially worse mechanism. If the full window agrees, extend the same experiment using a fresh immutable ID; never infer full solve equality.',
         test_consideration='No new solver instrumentation or parser behavior. Existing decision-prefix tests cover schema, truncated rows, suffix accounting with zero or nonzero overlap, and first differences. New solver changes require fresh unit-test consideration.',
         limitation='Stopped partial proofs are hash-bound and removed, not checked final certificates. No timing acceptance, denominator refresh, source/default change or restoration of a legitimate deleted binary.',
         dependencies=dependencies)
m['hashes'] = {str(p.relative_to(c)): sha(p) for p in [*(c / 'pins').iterdir(), c / 'run.py', c / 'array.sbatch']}
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
for path in (c / 'pins').iterdir():
    path.chmod(path.stat().st_mode & ~0o222)
print(c)

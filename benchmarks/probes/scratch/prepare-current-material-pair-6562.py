#!/usr/bin/env python3
"""Current Tactix selected-pair lifecycle and independent same-binary control."""
import json
from pathlib import Path
import shutil
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha

old = root / 'benchmarks/runs/20260909-6211-rank50-extended-prefix'
build = root / 'benchmarks/runs/20260909-6424-activation-stamp-build/manifest.json'
b = json.loads(build.read_text())
for rel, digest in b['files'].items():
    assert sha(root / b['source'] / rel) == digest, rel
binary = root / 'build/pin-6424-activation-stamp-trace/tactix'
assert sha(binary) == b['binaries'][str(binary.relative_to(root))]
c = root / 'benchmarks/runs/20260909-6562-rank50-current-material-pair'
resume = c.exists()
if resume:
    # The first preparation stopped on an incorrect authority pathname,
    # before writing a manifest or submitting any solver. Resume only that scope.
    assert not (c / 'manifest.json').exists() and not (c / 'launch.json').exists()
    assert {str(p.relative_to(c)) for p in c.rglob('*') if p.is_file()} == {
        'run.py', 'array.sbatch', 'pins/candidate', 'pins/candidate_control'}
    for arm in ('candidate', 'candidate_control'):
        assert sha(c / 'pins' / arm) == sha(binary)
for name in ('pins', 'traces', 'results', 'logs'):
    (c / name).mkdir(parents=True, exist_ok=resume)
for arm in ('candidate', 'candidate_control'):
    if not resume:
        shutil.copy2(binary, c / 'pins' / arm)
runner = (old / 'run.py').read_text().replace('tactix-6211-', 'tactix-6562-')
assert runner.count("('candidate', 'kissat')") == 1
runner = runner.replace("('candidate', 'kissat')", "('candidate', 'candidate_control')")
runner = runner.replace("prefix = 'KISSAT' if arm == 'kissat' else 'TACTIX'", "prefix = 'TACTIX'")
anchor = "assert not any(key.endswith('_EVENT_TRACE') for key in env)"
assert runner.count(anchor) == 1
runner = runner.replace(anchor, "if arm == 'candidate':\n    env['TACTIX_BINARY_LIFECYCLE_TRACE'] = str(c / 'traces/candidate.lifecycle.tsv')\n    env['TACTIX_BINARY_LIFECYCLE_PAIRS'] = m['selected_pair']\n" + anchor)
compile(runner, str(c / 'run.py'), 'exec')
if resume:
    assert (c / 'run.py').read_text() == runner
else:
    (c / 'run.py').write_text(runner)
batch = (old / 'array.sbatch').read_text().replace(str(old), str(c)).replace('6211-rank50-prefix', '6562-current-material-pair')
assert '--array=0-1' in batch and '--cpus-per-task=12' in batch and '--mem=48000M' in batch
if resume:
    assert (c / 'array.sbatch').read_text() == batch
else:
    (c / 'array.sbatch').write_text(batch)
prior = json.loads((old / 'manifest.json').read_text())
m = {k: prior[k] for k in ('rank', 'input', 'input_sha256_compressed', 'stop_conflict', 'stream_from', 'common_decisions', 'resource_contract', 'selection_sha256')}
authorities = [str(build.relative_to(root)),
    'benchmarks/runs/20260909-6556-rank50-material-pair/summary-6558.json',
    'benchmarks/runs/20260909-6556-rank50-material-pair/results/kissat.json',
    'benchmarks/runs/20260909-6439-activation-ranks1-50/results/049.json']
m.update(selected_pair='574,-525', sources={arm: str(binary.relative_to(root)) for arm in ('candidate', 'candidate_control')},
    preparation_failure='Initial preparation stopped before manifest/submission: FileNotFoundError for mistyped 6439-activation-stamp-ranks1-50 authority. Exact scaffold/pins/runner/batch verified unchanged before resuming with 6439-activation-ranks1-50; no solver ran or result was excluded.',
    earlier_closure={rel: sha(root / rel) for rel in authorities},
    source_manifest=str(build.relative_to(root)),
    native_campaign='benchmarks/runs/20260909-6556-rank50-material-pair',
    purpose='Confirm the native material-pair witness on current 6424 Tactix, with an independent same-binary observer-free control.',
    hypothesis='Current Tactix creates the selected pair at the native pre-divergence conflict but does not perform the native 823562 Eliminate boundary.',
    validation='Require complete ordinary decision, boundary, macro (if emitted), prefix and partial-certificate identity between current arms before inference. Validate inherited 139633-row hash236dcfa1a362af4d, then parse every lifecycle record and compare the full current/native decision streams. Cached native6556 is a semantic control only; no new timing reference.',
    test_consideration='No source or observer changes. Existing lifecycle/prefix readers and fixed tests reused. Add a regression test if new parsing or policy logic is introduced; no policy logic in this diagnostic.',
    limitation='Stopped diagnostic, not checked final proof or timing acceptance. Current pair lifecycle does not alone prove material-tracker causality. No admission observer exists in this current binary. Prior campaigns and fixed reference vector untouched.')
m['hashes'] = {str(p.relative_to(c)): sha(p) for p in [c / 'run.py', c / 'array.sbatch', *sorted((c / 'pins').iterdir())]}
with (c / 'manifest.json').open('x') as out:
    json.dump(m, out, indent=2)
    out.write('\n')
for p in (c / 'pins').iterdir():
    p.chmod(p.stat().st_mode & ~0o222)
print(c)

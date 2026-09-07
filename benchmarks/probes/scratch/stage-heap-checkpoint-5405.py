#!/usr/bin/env python3
"""Stage explicit owned evidence; never binaries, huge streams or active logs."""
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[3]
paths = {root / 'PROGRESS.md'}
raw_local = set()
for name in ('2026-09-07-rank35-heap-localization.md',
             '2026-09-07-heap-localization-artifact-index.json',
             '2026-09-07-heap-localization-validation.json',
             '2026-09-07-heap-localization-main-ctest.log'):
    paths.add(root / 'benchmarks/reports' / name)
for n in range(5396, 5406):
    paths.update((root / 'benchmarks/probes/scratch').glob(f'*-{n}.py'))
    c = next((root / 'benchmarks/runs').glob(f'20260907-{n}-*'))
    for p in c.rglob('*'):
        if not p.is_file() or 'pins' in p.relative_to(c).parts or '__pycache__' in p.parts:
            continue
        if p.name.endswith(('.decisions.tsv', '.deact.tsv', '.choice.tsv', '.heap.tsv')):
            raw_local.add(p)
            continue
        if p.stat().st_size > 5_000_000:
            continue
        if p.suffix in ('.json', '.py', '.sbatch', '.patch', '.tsv', '.log', '.out'):
            paths.add(p)
for n in (5383, 5389, 5393, 5395):
    c = next((root / 'benchmarks/runs').glob(f'20260907-{n}-*'))
    paths.update((c / 'results').glob('*.json'))
    if (c / 'summary.json').exists(): paths.add(c / 'summary.json')
assert all(p.exists() and p.is_relative_to(root) for p in paths)
ordered = [str(p.relative_to(root)) for p in sorted(paths)]
print(f'Staging {len(ordered)} explicit owned paths, {sum(p.stat().st_size for p in paths)} bytes', flush=True)
# These paths were introduced only by this checkpoint. Keep their full bytes
# local, with hashes and normalized witnesses in the staged authorities.
tracked = set(subprocess.run(['git', 'diff', '--cached', '--name-only', '--diff-filter=A'], cwd=root, capture_output=True, text=True, check=True).stdout.splitlines())
unstage = [str(p.relative_to(root)) for p in sorted(raw_local) if str(p.relative_to(root)) in tracked]
if unstage:
    subprocess.run(['git', 'rm', '--cached', '--quiet', '--', *unstage], cwd=root, check=True)
subprocess.run(['git', 'add', '-f', '--', *ordered], cwd=root, check=True)

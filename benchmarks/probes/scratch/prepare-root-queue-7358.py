#!/usr/bin/env python3
"""Add existing complete queue snapshots to the checked selected-heap probe."""
import json
from pathlib import Path
import shutil
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
old = root / 'benchmarks/runs/20260911-7356-bve-heap'
c = root / 'benchmarks/runs/20260911-7358-root-queue'
assert not c.exists()
m = json.loads((old / 'manifest.json').read_text())
deps = dict(m['dependencies'])
def bind(path, expected=None):
    digest = sha(path)
    assert expected is None or digest == expected, path
    deps[str(path.relative_to(root))] = digest
for rel, digest in list(deps.items()): bind(root / rel, digest)
for rel, digest in m['pin_sha256'].items(): bind(old / rel, digest)
for name in ('manifest.json', 'launch.json'): bind(old / name)
for task in range(2):
    path = old / f'results/{task:03d}.json'
    r = json.loads(path.read_text())
    assert classify(r, True)['checked']
    bind(path)
    for artifact in r['trace_artifacts'].values(): bind(old / artifact['path'], artifact['sha256'])
for folder in ('pins', 'logs', 'claims', 'provisional', 'results', 'traces'):
    (c / folder).mkdir(parents=True)
for path in (old / 'pins').iterdir():
    if path.name != 'runtime_task.py': shutil.copy2(path, c / 'pins' / path.name)
shutil.copy2(old / 'matrix.tsv', c / 'matrix.tsv')
runner = (old / 'pins/runtime_task.py').read_text()
replacements = {
    "('decisions', 'macro', 'active', 'inprocess', 'heap')":
        "('decisions', 'macro', 'active', 'inprocess', 'heap', 'queue')",
    '        solve = measured(command, stdout, stderr,':
        "        os.environ['TACTIX_QUEUE_STAMP_TRACE'] = str(trace_files['queue'])\n        solve = measured(command, stdout, stderr,",
    '        verdict, statistics = solver_summary(stdout)':
        "        del os.environ['TACTIX_QUEUE_STAMP_TRACE']\n        verdict, statistics = solver_summary(stdout)",
}
for before, after in replacements.items():
    assert runner.count(before) == 1, before
    runner = runner.replace(before, after)
compile(runner, str(c / 'pins/runtime_task.py'), 'exec')
with (c / 'pins/runtime_task.py').open('x') as out: out.write(runner)
batch = (old / 'array.sbatch').read_text().replace(str(old), str(c)).replace('7356-bve-heap', '7358-root-queue')
with (c / 'array.sbatch').open('x') as out: out.write(batch)
for path in (Path(__file__), Path(__file__).with_name('check-submit-root-queue-7358.py'),
             root / 'benchmarks/reports/2026-09-11-bve-heap-analysis-7357.json'):
    bind(path)
m.update(schema='tactix-root-queue-7358-v1', dependencies=deps,
    hypothesis='Whole prior macro streams first differ in queue_stamp at Reduce C14492: parent41753 versus candidate41724, both unassigned41724. Identify the complete pre/post queue membership and stamp remapping; determine whether 29 retained fixed members explain downstream BVE selected stamp -29. Existing queue observer only; no source edits or global trace.',
    acceptance='Exact entire7356 five archives and complete own Release statistics/certificate before interpreting added queue snapshots. Expected proofs checked/clean before acceptance. No runtime claim.',
    test_consideration='Validate snapshot headers, unique ordered external identities and numeric stamps; preserve non-selected snapshots. Compare complete pre/post queues at C14492, not only variable22861. Negative parser tests required. If identities appear fixed, independently establish fixed state rather than infer from queue count alone.')
m['pin_sha256'] = {str(path.relative_to(c)): sha(path)
                  for path in [*(c / 'pins').iterdir(), c / 'matrix.tsv', c / 'array.sbatch']}
with (c / 'manifest.json').open('x') as out: json.dump(m, out, indent=2)
for path in (c / 'pins').iterdir(): path.chmod(path.stat().st_mode & ~0o222)
print(c)

#!/usr/bin/env python3
"""Reuse the exact observer binary to test whether both events name one pair."""
import json
from pathlib import Path
import shutil
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
parent = root / 'benchmarks/runs/20260909-6553-rank50-material-producers'
c = root / 'benchmarks/runs/20260909-6556-rank50-material-pair'
assert not c.exists()
m = json.loads((parent / 'manifest.json').read_text())
summary = json.loads((parent / 'summary-6555.json').read_text())
assert summary['ordinary_replay_exact'] and summary['scratch_deleted']
for rel,digest in m['hashes'].items(): assert sha(parent / rel) == digest, rel
for name in ('pins', 'logs', 'results', 'traces'): (c / name).mkdir(parents=True)
shutil.copy2(parent / 'pins/kissat', c / 'pins/kissat')
runner = (parent / 'run.py').read_text().replace('tactix-6553-', 'tactix-6556-')
anchor = "assert not any(key.endswith('_EVENT_TRACE') for key in env)"
assert runner.count(anchor) == 1
runner = runner.replace(anchor, '''env['KISSAT_BINARY_LIFECYCLE_TRACE'] = str(c / 'traces/kissat.lifecycle.tsv')
env['KISSAT_BINARY_LIFECYCLE_PAIRS'] = m['selected_pair']
''' + anchor)
(c / 'run.py').write_text(runner)
batch = (parent / 'array.sbatch').read_text().replace(str(parent), str(c)).replace('6553-rank50-material', '6556-material-pair')
(c / 'array.sbatch').write_text(batch)
m.update(selected_pair='574,-525',
    cached_control=dict(campaign=str(parent.relative_to(root)), result_sha256=sha(parent / 'results/kissat.json'), summary_sha256=sha(parent / 'summary-6555.json')),
    purpose='Resolve whether the two observed new-binary material events refer to the same physical binary pair. Existing selected-pair lifecycle observer; no source edit.',
    validation='Require every6553 ordinary stream plus material stream and partial proof exactly; parse all selected-pair lifecycle records. No adjacency-based same-clause assumption or final-proof acceptance.',
    test_consideration='Same binary and existing lifecycle observer/parser with fixed tests; no new solver code. Prior callback and nine material-reader tests retained.')
m['hashes'] = {str(p.relative_to(c)): sha(p) for p in (c / 'run.py', c / 'array.sbatch', c / 'pins/kissat')}
with (c / 'manifest.json').open('x') as out:
    json.dump(m, out, indent=2)
    out.write('\n')
(c / 'pins/kissat').chmod((c / 'pins/kissat').stat().st_mode & ~0o222)
print(c)

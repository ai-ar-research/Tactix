#!/usr/bin/env python3
"""Observe one remaining omission, retaining immutable parent pins and runners."""
import json
from pathlib import Path
import shutil
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
old = root / 'benchmarks/runs/20260909-6248-repaired-rank32-membership'
native = root / 'benchmarks/runs/20260909-6208-rank32-schedule-membership'
c = root / 'benchmarks/runs/20260909-6256-rank32-remaining-retry'
assert not c.exists()
s = json.loads((old / 'summary.json').read_text())
assert s['same_binary_parent_prefix'] and not s['shared_score_or_occurrence_differences']
assert 399915 in [r['external'] for r in s['kissat_only']]
m = json.loads((old / 'manifest.json').read_text())
n = json.loads((native / 'manifest.json').read_text())
for source, manifest in ((old, m), (native, n)):
    for rel, digest in manifest['hashes'].items(): assert sha(source / rel) == digest
for name in ('pins', 'traces', 'logs', 'results'): (c / name).mkdir(parents=True)
for arm, source in (('candidate', old), ('kissat', native)):
    shutil.copy2(source / 'pins' / arm, c / 'pins' / arm)
    (c / 'pins' / arm).chmod((c / 'pins' / arm).stat().st_mode & ~0o222)
runner = (old / 'run.py').read_text()
changes = {
    'tactix-6248-': 'tactix-6256-',
    "env['SAT_RETRY_PRODUCER_EXTERNAL'] = '337674'": "env['SAT_RETRY_PRODUCER_EXTERNAL'] = '399915'",
    "env['TACTIX_RETRY_PRODUCER_TRACE'] = str(c / 'traces/candidate.retry-producer.tsv')":
        "env[prefix + '_RETRY_PRODUCER_TRACE'] = str(c / 'traces' / (arm + '.retry-producer.tsv'))",
    "env['TACTIX_BVE_SCHEDULE_TRACE'] = str(c / 'traces/candidate.schedule.tsv')":
        "env[prefix + '_BVE_SCHEDULE_TRACE'] = str(c / 'traces' / (arm + '.schedule.tsv'))",
    "(c / 'traces/candidate.schedule.tsv')": "(c / 'traces' / (arm + '.schedule.tsv'))",
}
for before, after in changes.items():
    assert runner.count(before) == 1, before
    runner = runner.replace(before, after)
compile(runner, str(c / 'run.py'), 'exec')
(c / 'run.py').write_text(runner)
batch = (old / 'array.sbatch').read_text().replace(str(old), str(c)).replace('6248-rank32', '6256-rank32')
assert batch.count('#SBATCH --array=0\n') == 1
(c / 'array.sbatch').write_text(batch.replace('#SBATCH --array=0\n', '#SBATCH --array=0-1\n'))
m['resource_contract']['array'] = '0-1'
m.pop('cached_native')
m.update(subject=399915,
    purpose='Localize one remaining missing retry producer after6231; choose a low-occurrence omission for minimal downstream fixture.',
    hypothesis='The remaining399915 omission has a missing removal event or a different flag-clear lifecycle; do not assume the337674 repair covers it.',
    parent_summary=dict(path=str((old / 'summary.json').relative_to(root)), sha256=sha(old / 'summary.json')),
    native_parent=dict(path=str((native / 'summary.json').relative_to(root)), sha256=sha(native / 'summary.json')),
    native_authority=n['observer_build'],
    validation='Candidate must replay6248 full prefix/boundary/macro/schedule and partial-proof bytes exactly; only selected retry journal changes. Native must match6208 pre-stop prefixes and full member schedule, followed by five selected rows. Verify300K hashes both arms. Different native stops prohibit partial-proof identity claims. Validate selected399915 schema before interpreting source histories.',
    test_consideration='No solver changes; tested existing selectors. New generator asserts exact replacements and syntax. Need producer parser negative controls for new subject; acceptance and timing are not inferred from this diagnostic.')
m['hashes'] = {rel: sha(c / rel) for rel in ('pins/candidate', 'pins/kissat', 'run.py', 'array.sbatch')}
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
print(c)

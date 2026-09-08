#!/usr/bin/env python3
"""Localize the latest complete-view largest loss on the current tested source."""
import datetime
import hashlib
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
runs = root / 'benchmarks/runs'
old = runs / '20260908-5726-root-reuse-rank33-prefix'
c = runs / '20260908-5735-root-reuse-rank31-prefix'
authority = runs / '20260908-5718-root-conflict-reuse'
full = runs / '20260908-5712-assignment-level-ranks1-40'
def sha(p):
    h = hashlib.sha256()
    with p.open('rb') as stream:
        for block in iter(lambda: stream.read(8 << 20), b''): h.update(block)
    return h.hexdigest()
m = json.loads((old / 'manifest.json').read_text())
for rel, digest in m['hashes'].items(): assert sha(old / rel) == digest
a = json.loads((authority / 'manifest.json').read_text())
for rel, digest in a['files'].items(): assert sha(root / a['source'] / rel) == digest
assert sha(old / 'pins/candidate') == a['binaries']['build/pin-5719-root-conflict-reuse-trace/tactix']
selection = full / 'next-losses.json'
loss = json.loads(selection.read_text())['losses'][0]
assert loss['rank'] == 31
checkpoint = json.loads((full / 'solver-checkpoint.json').read_text())
r = checkpoint['per_rank']['31']['tactix']
assert sha(root / r['record']) == r['record_sha256']
instance = json.loads((root / r['record']).read_text())['instance']
runner = (old / 'run.py').read_text().replace('tactix-5726-', 'tactix-5735-')
start = runner.index("selector = 'congruence_end'")
end = runner.index("assert not any(key.endswith('_EVENT_TRACE')", start)
runner = runner[:start] + runner[end:]
c.mkdir(exist_ok=False)
for name in ('pins', 'logs', 'traces', 'results'): (c / name).mkdir()
for arm in ('candidate', 'kissat'): shutil.copy2(old / 'pins' / arm, c / 'pins' / arm)
(c / 'run.py').write_text(runner)
(c / 'array.sbatch').write_text((old / 'array.sbatch').read_text().replace(str(old), str(c)))
m.update(created_at=datetime.datetime.now(datetime.timezone.utc).isoformat(),
    parent_campaign=str(old.relative_to(root)), predecessor=str(old.relative_to(root)),
    input='benchmarks/cache/' + instance, stop_conflict=300000, stream_from=0,
    common_decisions=1,
    hypothesis='Rank31 is the largest complete5672 time loss (+176.532s), with 38.44% extra conflicts and2.137x peak memory. Localize the earliest portable decision divergence on current root-reuse5718 before choosing the next general repair.',
    validation='Two clean300K stops, complete decision rows from0, prefix/stream consistency, immutable source/pin/input hashes and scratch cleanup. No parent replay assumption: the earlier5459 100K agreement is historical, not current-source evidence.',
    selection_authority=dict(path=str(selection.relative_to(root)),sha256=sha(selection)),
    limitation='Stopped proof-on causal diagnostic, not acceptance timing, a full verdict or a checked proof. Current5718 remains unfuzzed; B28+C5 and all40 remain mandatory.',
    test_consideration='Configuration-only reuse of tested current trace binaries and stopped runner; remove unrelated formula-dump selectors. Existing complete-stream comparator verifies both arms and prefix consistency; fixed91/91 Release and90/90 trace suites cover unchanged solver source.')
for key in ('selected_authority', 'parent_summary_sha256', 'prior_summary_sha256'):
    m.pop(key, None)
m['input_sha256_compressed'] = sha(root / m['input'])
m['hashes'] = {str(p.relative_to(c)):sha(p) for p in
    [*(c / 'pins').iterdir(), c / 'run.py', c / 'array.sbatch']}
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
for p in (c / 'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
print(c)

#!/usr/bin/env python3
"""Trace the exact residual binary's provenance using existing observers."""
import datetime as dt
import json
from pathlib import Path
import shutil
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
old = root / 'benchmarks/runs/20260909-6629-rank35-reorder-input'
c = root / 'benchmarks/runs/20260909-6637-rank35-binary-life'
assert not c.exists()
s = json.loads((old / 'summary-6631.json').read_text())
assert s['complete_own_replay'] and s['score_reconstruction_exact'] and s['full_large_rows_equal']
assert s['previous_only_binary'] == [dict(row=[-29898, -1373], copies=1)]
assert not s['candidate_only_binary']
for rel, digest in s['hashes'].items():
    assert sha(root / rel) == digest, rel
m = json.loads((old / 'manifest.json').read_text())
for rel, digest in m['hashes'].items():
    assert sha(old / rel) == digest, rel
runner = (old / 'run.py').read_text().replace('tactix-6629-', 'tactix-6637-')
anchor = "assert not any(key.endswith('_EVENT_TRACE') for key in env)"
assert runner.count(anchor) == 1
runner = runner.replace(anchor, "env.update(TACTIX_BINARY_LIFECYCLE_TRACE=str(c / 'traces' / (arm + '.lifecycle.tsv')),\n           TACTIX_BINARY_LIFECYCLE_PAIRS='-29898,-1373')\n" + anchor)
compile(runner, str(c / 'run.py'), 'exec')
for folder in ('pins', 'logs', 'results', 'traces'):
    (c / folder).mkdir(parents=True)
for arm in ('candidate', 'previous'):
    shutil.copy2(old / 'pins' / arm, c / 'pins' / arm)
    p = old / f'results/{arm}.json'
    r = json.loads(p.read_text())
    m['dependencies'][str(p.relative_to(root))] = sha(p)
    for name, digest in r['trace_sha256'].items():
        m['dependencies'][str((old / 'traces' / name).relative_to(root))] = digest
summary = old / 'summary-6631.json'
m['dependencies'][str(summary.relative_to(root))] = sha(summary)
(c / 'run.py').write_text(runner)
(c / 'array.sbatch').write_text((old / 'array.sbatch').read_text().replace(str(old), str(c)).replace('6629-reorder-input', '6637-binary-life'))
m.update(created_at=dt.datetime.now(dt.timezone.utc).isoformat(), cached_controls=str(old.relative_to(root)),
    selected_pairs=[[-29898, -1373]],
    purpose='The one extra earlier binary(-29898,-1373) exactly explains1373 score residual;25 incident large clauses/physical metadata/root values and all58822 active identities agree. Trace existing selected-pair lifecycle to identify its first mutation/producer, not restore a possibly redundant clause for a cheaper branch.',
    validation='Require all6629 ordinary, heap, choice, score and selected-input traces plus partial-proof bytes/hash to replay exactly. Validate complete v3 lifecycle with maintained reader. Distinguish metadata/internal ids from portable event/rule/values/copy changes; restrict causality to before Reorder3074062.',
    test_consideration='No solver/observer source edit;existing binary_lifecycle reader has5 tests. Reorder-input reconstruction now has9 tests and183 benchmark testsPASS. New event-specific assertions need fixtures before interpretation.',
    limitation='Observed mutation provenance only, not whole-formula identity, native parity or runtime improvement. Preserve compact-stamp/activation repairs and Rank32/33 closures; do not restore a removed binary without a general source-backed defect.')
m['hashes'] = {name: sha(c / name) for name in ('pins/candidate', 'pins/previous', 'run.py', 'array.sbatch')}
with (c / 'manifest.json').open('x') as stream:
    json.dump(m, stream, indent=2); stream.write('\n')
for p in (c / 'pins').iterdir():
    p.chmod(p.stat().st_mode & ~0o222)
print(c)

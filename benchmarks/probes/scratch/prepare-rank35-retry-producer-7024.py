#!/usr/bin/env python3
"""Trace the earliest attempted extra schedule member's retry provenance."""
import json
from pathlib import Path
import shutil
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
old = root / 'benchmarks/runs/20260910-7018-rank35-bve-frontier'
c = root / 'benchmarks/runs/20260910-7024-rank35-retry-producer'
assert not c.exists()
m = json.loads((old / 'manifest.json').read_text())
s = json.loads((old / 'attempt-alignment-7023.json').read_text())
external = s['first_extra_member_attempt']['candidate']['begin']['external']
assert external == 174970
dependencies = {}
def bind(p, expected=None):
    actual = sha(p)
    assert expected is None or actual == expected, p
    dependencies[str(p.relative_to(root))] = actual
for rel, digest in m['hashes'].items(): bind(old / rel, digest)
for rel, digest in m['dependencies'].items(): bind(root / rel, digest)
for name in ('manifest.json', 'schedule-summary-7020.json', 'candidate-summary-7021.json', 'attempt-alignment-7023.json'):
    bind(old / name)
arms = ('candidate_schedule', 'kissat_schedule')
for arm in arms:
    p = old / 'results' / (arm + '.json'); bind(p)
    r = json.loads(p.read_text())
    assert r['exit_code'] == 0 and r['scratch_deleted']
    for name, digest in r['trace_sha256'].items(): bind(old / 'traces' / name, digest)
    prefix = b'TACTIX' if arm.startswith('candidate') else b'KISSAT'
    assert prefix + b'_RETRY_PRODUCER_TRACE\0' in (old / 'pins' / arm).read_bytes()
bind(Path(__file__))
for name in ('pins', 'logs', 'traces', 'results'): (c / name).mkdir(parents=True)
for arm in arms: shutil.copy2(old / 'pins' / arm, c / 'pins' / arm)
runner = (old / 'run.py').read_text()
before = "('candidate', 'kissat', 'candidate_schedule', 'kissat_schedule')[int(sys.argv[1])]"
assert runner.count(before) == 1
runner = runner.replace(before, repr(arms) + '[int(sys.argv[1])]').replace('tactix-7018-', 'tactix-7024-')
anchor = "assert not any(key.endswith('_EVENT_TRACE') for key in env)"
extra = """env[prefix + '_RETRY_PRODUCER_TRACE'] = str(c / 'traces' / (arm + '.retry-producer.tsv'))
env['SAT_RETRY_PRODUCER_FROM'] = '0'
env['SAT_RETRY_PRODUCER_TO'] = '387253'
env['SAT_RETRY_PRODUCER_EXTERNAL'] = '174970'
env[prefix + '_BVE_CANDIDATE_TRACE'] = str(c / 'traces' / (arm + '.bve-history.tsv'))
env['SAT_BVE_CANDIDATE_EXTERNAL'] = '174970'
"""
assert runner.count(anchor) == 1
(c / 'run.py').write_text(runner.replace(anchor, extra + anchor))
batch = (old / 'array.sbatch').read_text().replace(str(old), str(c)).replace('7018-rank35-bve-frontier', '7024-rank35-retry-producer').replace('--array=0-3', '--array=0-1')
(c / 'array.sbatch').write_text(batch)
out = {key: m[key] for key in ('rank', 'input', 'input_sha256_compressed', 'stop_conflict', 'stream_from', 'common_decisions', 'prior_common_hash', 'session_authority')}
out.update(resource_contract=dict(m['resource_contract'], array='0-1'), dependencies=dependencies,
    arms=list(arms), parent=str(old.relative_to(root)), selected_external=external,
    hypothesis='Current BVE starts with21 extra retry-eligible variables despite exact shared scores/counts. Trace174970, the earliest attempted extra member, through all prior mark producers and attempts before the selected387253 schedule stop. It is a representative witness, not proof all21 share one cause.',
    validation='Require exact own7018 complete schedule, decision, boundary/macro traces and partial-proof bytes/hash before interpreting only two new selected journals. Preserve missing native macro capability. Current/native own raw root counters are not equivalent.',
    test_consideration='Existing compile-time observers, no solver or parser edit. Reused schedule/prefix and candidate schema fixtures cover framing; exact whole-parent replay is mandatory.',
    limitation='No blanket feature disable, specific-variable production selector, source change or timing acceptance. Do not restore rejected cleanup5920; its score hypothesis is unsupported here because every shared score matches.')
out['hashes'] = {str(p.relative_to(c)): sha(p) for p in [*(c / 'pins').iterdir(), c / 'run.py', c / 'array.sbatch']}
with (c / 'manifest.json').open('x') as stream:
    json.dump(out, stream, indent=2); stream.write('\n')
for p in (c / 'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
print(c)

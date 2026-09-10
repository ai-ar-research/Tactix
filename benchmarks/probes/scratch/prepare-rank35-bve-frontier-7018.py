#!/usr/bin/env python3
"""Observe order/effect/budget and initial physical BVE frontier independently."""
import json
from pathlib import Path
import shutil
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
old = root / 'benchmarks/runs/20260910-7011-rank35-current-native-prefix'
c = root / 'benchmarks/runs/20260910-7018-rank35-bve-frontier'
assert not c.exists()
m = json.loads((old / 'manifest.json').read_text())
dependencies = {}
def bind(path, expected=None):
    digest = sha(path)
    assert expected is None or digest == expected, path
    dependencies[str(path.relative_to(root))] = digest
for rel, digest in m['hashes'].items(): bind(old / rel, digest)
for rel, digest in m['dependencies'].items(): bind(root / rel, digest)
for name in ('summary-7015.json', 'boundary-localization-7017.json', 'manifest.json'):
    p = old / name; bind(p)
    for rel, digest in json.loads(p.read_text()).get('hashes', {}).items():
        bind(old / rel if name == 'manifest.json' else root / rel, digest)
for arm in ('candidate', 'kissat'):
    p = old / 'results' / (arm + '.json'); bind(p)
    r = json.loads(p.read_text())
    assert r['exit_code'] == 0 and r['scratch_deleted']
    for name, digest in r['trace_sha256'].items(): bind(old / 'traces' / name, digest)
    # Require real observer capability in both unchanged pins before launch.
    data = (old / 'pins' / arm).read_bytes()
    prefix = b'TACTIX' if arm == 'candidate' else b'KISSAT'
    for suffix in (b'_BVE_CANDIDATE_TRACE', b'_BVE_SCHEDULE_TRACE'):
        assert prefix + suffix + b'\0' in data
for rel in ('tools/bench/bve_schedule.py', 'tools/bench/test_bve_schedule.py',
            'tools/bench/decision_prefix.py', 'tools/bench/test_decision_prefix.py'):
    bind(root / rel)
bind(Path(__file__))
for name in ('pins', 'logs', 'results', 'traces'): (c / name).mkdir(parents=True)
arms = ('candidate', 'kissat', 'candidate_schedule', 'kissat_schedule')
for arm in arms: shutil.copy2(old / 'pins' / arm.split('_')[0], c / 'pins' / arm)
runner = (old / 'run.py').read_text().replace("('candidate', 'kissat')[int(sys.argv[1])]", repr(arms) + '[int(sys.argv[1])]')
runner = runner.replace("arm == 'kissat'", "arm.startswith('kissat')").replace('tactix-7011-', 'tactix-7018-')
anchor = "assert not any(key.endswith('_EVENT_TRACE') for key in env)"
assert runner.count(anchor) == 1
extra = """if arm.endswith('_schedule'):
    env[prefix + '_BVE_SCHEDULE_TRACE'] = str(c / 'traces' / (arm + '.bve-schedule.tsv'))
    env['SAT_BVE_SCHEDULE_CONFLICT'] = '387253'
    env['SAT_BVE_SCHEDULE_ORDINAL'] = '0'
    env['SAT_BVE_SCHEDULE_MEMBERS'] = '1'
else:
    env[prefix + '_BVE_CANDIDATE_TRACE'] = str(c / 'traces' / (arm + '.bve-candidates.tsv'))
    env['SAT_BVE_CANDIDATE_CONFLICT'] = '387253'
"""
runner = runner.replace(anchor, extra + anchor)
stop = "assert (c / 'traces' / (arm + '.prefix.tsv')).exists(), 'Requested stop was not observed'"
assert runner.count(stop) == 1
runner = runner.replace(stop, "if arm.endswith('_schedule'):\n    assert (c / 'traces' / (arm + '.bve-schedule.tsv')).exists()\n    assert not (c / 'traces' / (arm + '.prefix.tsv')).exists()\nelse:\n    " + stop)
(c / 'run.py').write_text(runner)
batch = (old / 'array.sbatch').read_text().replace(str(old), str(c)).replace('7011-rank35-current-native-prefix', '7018-rank35-bve-frontier').replace('--array=0-1', '--array=0-3')
assert '--array=0-3\n' in batch and '--nodelist' not in batch
(c / 'array.sbatch').write_text(batch)
out = {key: m[key] for key in ('rank', 'input', 'input_sha256_compressed', 'stop_conflict', 'stream_from', 'common_decisions', 'prior_common_hash', 'session_authority')}
out.update(resource_contract=dict(m['resource_contract'], array='0-3'), dependencies=dependencies,
    arms=list(arms), parent=str(old.relative_to(root)), selected_conflict=387253,
    hypothesis='The BVE phase immediately preceding the first native decision split has different effects despite matching entry counts. Distinguish first physical schedule differences from candidate-local effects or budget differences; extra eliminations alone are not worse.',
    validation='Full1M candidate-observer arms must exactly replay each own7011 ordinary trace and partial-proof bytes/hash before interpreting new rows. Schedule arms intentionally stop before first candidate: require complete member framing, clean stop and emitted decision-stream prefix identity against own7011. Missing native macro observer remains explicit, not a failed solver. Preserve all earlier raw mismatches.',
    test_consideration='Only existing observer configuration changes; no solver/parser edit. Reuse complete-schedule and prefix/framing tests. Preparation checks real observer strings in both unchanged pins; two schedule stops are intentionally distinct from two1M stops.',
    limitation='Four independent stopped diagnostics, not acceptance timing or checked final proofs. No source/default change, native denominator refresh, promotion or fuzz access.')
out['hashes'] = {str(p.relative_to(c)): sha(p) for p in [*(c / 'pins').iterdir(), c / 'run.py', c / 'array.sbatch']}
with (c / 'manifest.json').open('x') as stream:
    json.dump(out, stream, indent=2); stream.write('\n')
for p in (c / 'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
print(c)

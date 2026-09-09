#!/usr/bin/env python3
"""Current/earlier Rank35 branch state, bound to the retained complete replay."""
import datetime as dt
import json
from pathlib import Path
import shutil
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify

c = root / 'benchmarks/runs/20260909-6619-material-rank35-choice'
assert not c.exists()
old = root / 'benchmarks/runs/20260909-6467-rank35-near-final-prefix'
summary = old / 'summary.json'
s = json.loads(summary.read_text())
assert s['decisions']['left'] == [3074183, 161631566, 1122, -482577, 0]
assert s['decisions']['right'] == [3074183, 161631566, 1122, -486307, 0]
for rel, digest in s['hashes'].items():
    assert sha(root / rel) == digest, rel
m = json.loads((old / 'manifest.json').read_text())
for rel, digest in m['hashes'].items():
    assert sha(old / rel) == digest, rel
authority = root / 'benchmarks/runs/20260909-6579-material-schedule-build/manifest.json'
b = json.loads(authority.read_text())
for rel, digest in b['files'].items():
    assert sha(root / b['source'] / rel) == digest, rel
pin = root / 'build/pin-6579-material-schedule-trace/tactix'
assert sha(pin) == b['binaries'][str(pin.relative_to(root))]
dependencies = {str(summary.relative_to(root)): sha(summary), str(authority.relative_to(root)): sha(authority)}
full_records = []
for name in ('6594-material-ranks1-50', '6439-activation-ranks1-50'):
    p = root / 'benchmarks/runs' / ('20260909-' + name) / 'results/034.json'
    r = json.loads(p.read_text())
    st = classify(r, True)
    assert st['checked'] and st['clean'] and r['rank'] == 35
    full_records.append(r)
    dependencies[str(p.relative_to(root))] = sha(p)
for key in ('verdict', 'certificate_sha256', 'certificate_bytes'):
    assert full_records[0][key] == full_records[1][key]
nt = lambda r: [x for x in r['solver_statistics'] if not x.startswith('c timing')]
assert nt(full_records[0]) == nt(full_records[1])
for arm in ('candidate', 'previous'):
    p = old / f'results/{arm}.json'
    r = json.loads(p.read_text())
    assert r['exit_code'] == 0 and r['scratch_deleted']
    dependencies[str(p.relative_to(root))] = sha(p)
    for rel, digest in r['trace_sha256'].items():
        p = old / 'traces' / rel
        assert sha(p) == digest, p
        dependencies[str(p.relative_to(root))] = digest
vivify = root / 'benchmarks/runs/20260909-6483-rank35-vivify-replay/summary.json'
dependencies[str(vivify.relative_to(root))] = sha(vivify)
runner = (old / 'run.py').read_text().replace('tactix-6467-', 'tactix-6619-')
anchor = "assert not any(key.endswith('_EVENT_TRACE') for key in env)"
assert runner.count(anchor) == 1
extra = """env.update(SAT_DECISION_HEAP_SNAPSHOT='1',
           SAT_EVENT_TRACE_HEAP_SET_AFTER_DECISION='1',
           SAT_EVENT_TRACE_HEAP_SET_PATH=str(c / 'traces' / (arm + '.heap.tsv')),
           SAT_EVENT_TRACE_HEAP_SET_CONFLICT='3074183',
           SAT_EVENT_TRACE_HEAP_SET_DECISION='161631565',
           SAT_CHOICE_STATE_CONFLICT='3074183', SAT_CHOICE_STATE_VARIABLE='486307')
env['TACTIX_CHOICE_STATE_TRACE'] = str(c / 'traces' / (arm + '.choice.tsv'))
"""
runner = runner.replace(anchor, extra + anchor)
compile(runner, str(c / 'run.py'), 'exec')
for folder in ('pins', 'logs', 'results', 'traces'):
    (c / folder).mkdir(parents=True)
shutil.copy2(pin, c / 'pins/candidate')
shutil.copy2(old / 'pins/previous', c / 'pins/previous')
(c / 'run.py').write_text(runner)
(c / 'array.sbatch').write_text((old / 'array.sbatch').read_text().replace(str(old), str(c)).replace('6467-rank35-near-final', '6619-rank35-choice'))
prior = json.loads((root / 'benchmarks/reports/2026-09-09-material-schedule-audit-6617.json').read_text())
m.update(created_at=dt.datetime.now(dt.timezone.utc).isoformat(),
    current_source=str(authority.relative_to(root)), cached_controls=str(old.relative_to(root)),
    dependencies=dependencies, protected_campaign_hashes=prior['protected_campaign_hashes'],
    selected_conflict=3074183, last_common_decision=161631565, wanted_variable=486307,
    purpose='Distinguish wanted-variable assignment/reason from branch heap membership/score/position at the first Rank35 split. Cached macro rows show stable mode; capture complete stable heap after last common decision, before subsequent propagation.',
    validation='First require current6579 and earlier6305 to replay every own6467 ordinary trace and partial-proof bytes/hash through4.4M. Current full35 work/certificate matches6424, but that alone is not a prefix equivalence proof. If any replay differs, stop interpretation and localize new prefix. Validate all choice rows and complete heap framing.',
    test_consideration='No solver/observer edit. Reuse existing standalone heap and choice observers; add strict maintained branch-state reader tests for malformed rows, duplicate identities, nonfinite scores, order, reason/assignment consistency and comparison categories.',
    limitation='Two Tactix arms, not native equivalence or refreshed timing. Snapshot follows last common assignment but precedes its propagation. Equal heap fields do not imply identical formulas. Partial proof replay is not final acceptance. No promotion, fuzz access or old campaign edit.')
m['authorities']['candidate'] = dict(path=str(authority.relative_to(root)), sha256=sha(authority))
m['hashes'] = {rel: sha(c / rel) for rel in ('pins/candidate', 'pins/previous', 'run.py', 'array.sbatch')}
with (c / 'manifest.json').open('x') as out:
    json.dump(m, out, indent=2)
    out.write('\n')
for p in (c / 'pins').iterdir():
    p.chmod(p.stat().st_mode & ~0o222)
print(c)

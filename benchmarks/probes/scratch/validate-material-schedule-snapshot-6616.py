#!/usr/bin/env python3
"""Compare current complete physical schedule, without reviving a rejected fix."""
import json
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from decision_prefix import first_stream_difference
from bve_schedule import parse

c = root / 'benchmarks/runs/20260909-6614-material-rank31-schedule'
target = c / 'summary-6616.json'
assert not target.exists()
m = json.loads((c / 'manifest.json').read_text())
hashes = {}
def bind(p, expected=None):
    digest = sha(p)
    if expected is not None:
        assert digest == expected, p
    hashes[str(p.relative_to(root))] = digest
    return digest
for rel, digest in m['hashes'].items():
    bind(c / rel, digest)
for rel, digest in m['dependencies'].items():
    bind(root / rel, digest)
for rel in ('manifest.json', 'launch.json', 'results/candidate.json'):
    bind(c / rel)
r = json.loads((c / 'results/candidate.json').read_text())
assert r['exit_code'] == 0 and r['scratch_deleted']
assert r['binary_sha256'] == sha(c / 'pins/candidate')
assert r['partial_certificate']['bytes'] > 0
assert not (c / 'traces/candidate.prefix.tsv').exists()
for rel, digest in r['trace_sha256'].items():
    bind(c / 'traces' / rel, digest)
control = root / m['cached_control']
prefix = first_stream_difference(c / 'traces/candidate.decisions.tsv',
                                 control / 'traces/candidate.decisions.tsv')
assert prefix['left'] is None and prefix['right'] == [179823, 76834766, 1, -126831, 0]
assert prefix['matching_rows'] == 76952471
schedules = {}
headers = {}
for arm, campaign, filename in (('current', c, 'candidate'),
        ('old', root / m['cached_schedule'], 'candidate'),
        ('native', root / m['cached_schedule'], 'kissat')):
    p = campaign / 'traces' / (filename + '.bve-schedule.tsv')
    bind(p)
    with p.open() as stream:
        headers[arm], schedules[arm] = parse(stream)
    assert headers[arm]['conflicts'] == '179823'
    assert headers[arm]['decisions'] == '76834765'
diffs = {}
for arm in ('old', 'native'):
    left = {r['external']: r for r in schedules['current']}
    right = {r['external']: r for r in schedules[arm]}
    diffs[arm] = dict(current_only=sorted(left.keys() - right.keys()),
        other_only=sorted(right.keys() - left.keys()),
        changed=[dict(current=left[v], other=right[v]) for v in left.keys() & right.keys()
                 if left[v] != right[v]])
    diffs[arm]['changed'].sort(key=lambda pair: int(pair['current']['position']))
same_old = schedules['current'] == schedules['old']
out = dict(headers=headers, decisions_before_snapshot=prefix,
    complete_schedule_equal_old=same_old, differences=diffs, hashes=hashes,
    decision=('Known omitted preconnection schedule witness reproduced. Do not tune heap ties or restore rejected5920 from prefix identity; retain its adverse Rank31/C runtime evidence.' if same_old else
              'Current schedule differs from the old witness; localize changed members/scores before selecting a repair.'),
    limitation='Intentional pre-first-candidate observer stop, not full300K observer replay or checked final proof. Equal schedule rows do not establish entire formula identity or full-run benefit.')
with target.open('x') as stream:
    json.dump(out, stream, indent=2)
    stream.write('\n')
print(json.dumps({k: v for k, v in out.items() if k != 'hashes'}, indent=2))

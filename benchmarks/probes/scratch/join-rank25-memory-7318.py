#!/usr/bin/env python3
"""Preserve whole-VMA RSS and mapping origins at each reproduced Rank25 peak."""
import json
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from memory_syscalls import parse
from memory_snapshot_join import join
assert sys.argv[1:] in ([], ['--seal'])
c = root / 'benchmarks/runs/20260911-7315-rank25-native-memory'
target = c / 'snapshot-origins-7318.json'
if '--seal' in sys.argv:
    assert not target.exists(), 'successful analysis frozen'
hashes = {}
def bind(p, expected=None):
    digest = sha(p)
    assert expected is None or digest == expected, p
    hashes[str(p.relative_to(root))] = digest
p = c / 'resident-analysis-7316.json'
bind(p)
data = json.loads(p.read_text())
for rel, digest in data['hashes'].items():
    bind(root / rel, digest)
result = {}
for task, arm in enumerate(('candidate', 'previous')):
    a = data['arms'][arm]
    assert a['exact_release_peak_reproduced']
    events, unknown = parse((c / f'logs/{task:03d}.memory-syscalls.txt').read_text())
    out = join(events, unknown, c / 'pins' / arm, a['maximum_snapshot'])
    assert out['total_rss_kib'] == a['maximum_snapshot']['smaps_total_rss_kib']
    result[arm] = out
    print(arm, 'topology stable', out['topology_stable_by_entry_timestamps'],
          'RSS', out['total_rss_kib'], 'calls', out['syscall_counts'])
    for region in sorted(out['regions'], key=lambda r:-r['kib']['Rss'])[:6]:
        print(region['name'] or '<anonymous>', region['kib'],
              [(o['event'], o['owner']) for o in region['origins']])
for p in (Path(__file__), root / 'tools/bench/memory_snapshot_join.py',
          root / 'tools/bench/test_memory_snapshot_join.py'):
    bind(p)
if '--seal' in sys.argv:
    with target.open('x') as out:
        json.dump(dict(hashes=hashes, arms=result), out, indent=2)
    print(target)

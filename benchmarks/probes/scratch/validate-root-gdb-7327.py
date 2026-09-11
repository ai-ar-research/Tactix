#!/usr/bin/env python3
"""Exact ordinary-binary snapshot gate; preserve failures, never weaken RSS."""
import importlib.util
import json
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root/'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
from proc_memory_task import status_values
spec = importlib.util.spec_from_file_location('fingerprint', Path(__file__).with_name('occurrence-fingerprint-7256.py'))
h = importlib.util.module_from_spec(spec); spec.loader.exec_module(h); h.fixtures()
assert sys.argv[1:] in ([], ['--seal'])
c = root/'benchmarks/runs/20260911-7327-rank25-root-gdb'
target = c/'summary.json'
if '--seal' in sys.argv:
    assert not target.exists(), 'successful summary frozen'
m = json.loads((c/'manifest.json').read_text())
hashes = {}
def bind(p, expected=None):
    digest = sha(p)
    assert expected is None or digest == expected, p
    hashes[str(p.relative_to(root))] = digest
bind(c/'manifest.json'); bind(c/'launch.json')
for rel, digest in m['pin_sha256'].items(): bind(c/rel, digest)
for rel, digest in m['evidence_sha256'].items(): bind(root/rel, digest)
records, states, snapshot = {}, {}, None
for folder in ('provisional', 'results'):
    for p in sorted((c/folder).glob('*.json')):
        r = json.loads(p.read_text()); bind(p)
        task = r['task']; arm = r['arm']
        assert task == int(p.stem) and task in (0, 1) and arm == ('candidate', 'previous')[task]
        assert r['rank'] == 25 and r['expected'] == 'SAT'
        state = classify(r, folder == 'results')
        assert state['status'] in ('pending_validation', 'checked_SAT'), (p, state)
        oracle = m['release_records'][arm]
        q = root/oracle['path']; bind(q, oracle['sha256'])
        assert h.raw_fingerprint(r) == h.raw_fingerprint(json.loads(q.read_text())), ('work/certificate', p)
        if folder == 'results': assert r['binary_sha256'] == m['arms'][arm]['sha256']
        if task in records: assert records[task]['solve'] == r['solve']
        if arm == 'candidate':
            e = r['solve']['root_gdb']; assert not e.get('missing')
            q = c/e['path']; assert q.parent == c/'logs' and q.stat().st_size == e['bytes'] > 0
            bind(q, e['sha256'])
            rows = [json.loads(line) for line in q.read_text().splitlines()]
            assert [row['kind'] for row in rows] == ['start', 'snapshot', 'done']
            assert rows[-1] == dict(kind='done', captures=1, errors=[], exits=[10])
            snapshot = rows[1]
            assert snapshot['pc'] == 0x40ed1f and snapshot['new_reconstruction_capacity_bytes'] == 474611712
            assert snapshot['old_reconstruction']['capacity_bytes'] == 237305856
            assert rows[0]['config_sha256'] == m['pin_sha256']['pins/root-gdb-config.json']
            snapshot['status_before_kib'] = status_values(snapshot['status_before'])
            snapshot['status_after_kib'] = status_values(snapshot['status_after'])
        records[task], states[task] = r, state
rss_gate = len(records) == 2 and all(r['solve']['peak_rss_kb'] == 2484880 for r in records.values())
print('GDB diagnostic', len(records), '/2 exact outcomes;', sum(s['checked'] for s in states.values()), 'checked-clean')
print('Exact ordinary RSS gate', rss_gate, {r['arm']:r['solve']['peak_rss_kb'] for r in records.values()})
if snapshot:
    print('snapshot populations', snapshot['state']['populations'], 'status', snapshot['status_before_kib'])
bind(Path(__file__))
if '--seal' in sys.argv:
    assert len(records) == 2 and snapshot, 'incomplete solver phase'
    with target.open('x') as out:
        json.dump(dict(hashes=hashes, records=records, states=states, snapshot=snapshot,
            exact_ordinary_rss_reproduced=rss_gate,
            limitation=m['limitation']), out, indent=2)
    print(target)

#!/usr/bin/env python3
"""Bind complete RSS/smaps evidence and bracket the resident high-water rise."""
import json
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from memory_syscalls import analyze, parse
from proc_memory_analysis import summarize
assert sys.argv[1:] in ([], ['--seal'])
c = root / 'benchmarks/runs/20260911-7299-proc-memory-rank31'
target = c / 'resident-analysis-7300.json'
if '--seal' in sys.argv:
    assert not target.exists(), 'successful analysis frozen'
if not (c / 'summary.json').exists():
    print('Await complete exact-work summary; do not freeze growing observations')
    raise SystemExit(0)
summary = json.loads((c / 'summary.json').read_text())
hashes = {}
def bind(p, expected=None):
    digest = sha(p)
    assert expected is None or digest == expected, p
    hashes[str(p.relative_to(root))] = digest
bind(c / 'summary.json')
for rel, digest in summary['hashes'].items():
    bind(root / rel, digest)
arms = {}
for task, arm in enumerate(('candidate', 'previous')):
    proc = c / f'logs/{task:03d}.proc-memory.jsonl'
    data = summarize([json.loads(line) for line in proc.read_text().splitlines()])
    events, unknown = parse((c / f'logs/{task:03d}.memory-syscalls.txt').read_text())
    virtual = analyze(events, unknown, c / 'pins' / arm, 4096)
    assert data['pid'] == virtual['pid']
    data['release_rss_kib'] = {'candidate': 562920, 'previous': 495156}[arm]
    data['gnu_time_rss_kib'] = summary['records'][str(task)]['solve']['peak_rss_kb']
    data['exact_release_peak_reproduced'] = data['release_rss_kib'] == data['gnu_time_rss_kib'] == data['peak_hwm_kib']
    # These are syscall-entry timestamps, not memory-touch or completion times.
    lower = data['previous_sample_after_ns']
    upper = data['maximum_sample_after_ns']
    indices = [i for i, e in enumerate(events) if (lower is None or e.time_us*1000 >= lower) and e.time_us*1000 <= upper]
    previous = [i for i, e in enumerate(events) if lower is not None and e.time_us*1000 < lower]
    following = [i for i, e in enumerate(events) if e.time_us*1000 > upper]
    context = sorted(set(indices + previous[-2:] + following[:2]))
    data['syscalls_near_detection_window'] = [dict(index=i, inside_window=i in indices,
        **vars(events[i])) for i in context]
    data['maximum_detection_elapsed_seconds'] = (upper-events[0].time_us*1000)/1e9
    data['traced_virtual_peak'] = virtual['peak']
    data['traced_final_extent'] = virtual['final']
    arms[arm] = data
    print(arm, 'Release/GNUtime/solverHWM', data['release_rss_kib'],
          data['gnu_time_rss_kib'], data['peak_hwm_kib'],
          'first detected seconds', data['maximum_detection_elapsed_seconds'])
    snapshot = data['maximum_snapshot']
    if snapshot:
        print('smaps totalRSS', snapshot['smaps_total_rss_kib'],
              'heap', [r['kib'] for r in snapshot['heap']],
              'HWM stable', snapshot['hwm_unchanged_through_snapshot'])
    for e in data['syscalls_near_detection_window']:
        print('syscall', e['index'], e['operation'], e['arguments'], e['result'],
              'inside', e['inside_window'])
for name in ('memory_syscalls.py', 'proc_memory_task.py', 'proc_memory_analysis.py',
             'test_memory_syscalls.py', 'test_proc_memory_task.py', 'test_proc_memory_analysis.py'):
    bind(root / 'tools/bench' / name)
bind(Path(__file__))
result = dict(hashes=hashes, arms=arms,
    limitation='No timing acceptance. Peak RSS reproduction is a prerequisite, not a substitute for allocator-object liveness or causality. Smaps is a later, non-atomic sampled state; syscall entries bracket a detection window, not individual page faults. Exact work/certificates permit provisional work while independent proofs remain pending.')
if '--seal' in sys.argv:
    with target.open('x') as out:
        json.dump(result, out, indent=2)
    print(target)

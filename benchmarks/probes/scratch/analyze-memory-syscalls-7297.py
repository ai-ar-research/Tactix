#!/usr/bin/env python3
"""Bind completed exact-Release traces and summarize virtual mapping ownership."""
import json
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from memory_syscalls import analyze, parse
assert sys.argv[1:] in ([], ['--seal'])
c = root / 'benchmarks/runs/20260911-7295-memory-syscalls-rank31'
target = c / 'mapping-analysis-7297.json'
if '--seal' in sys.argv:
    assert not target.exists(), 'successful analysis frozen'
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
    trace = c / f'logs/{task:03d}.memory-syscalls.txt'
    events, unknown = parse(trace.read_text())
    data = analyze(events, unknown, c / 'pins' / arm, 4096)
    data['solver_peak_rss_kib'] = summary['records'][str(task)]['solve']['peak_rss_kb']
    assert data['solver_peak_rss_kib'] == {'candidate': 562920, 'previous': 495156}[arm]
    assert not data['failed_syscalls']
    data['page_size'] = 4096
    data['peak_event'] = vars(events[data['peak']['event']])
    # Keep all source stack frames, not just the first solver-frame aggregation.
    for item in data['peak']['mapping_owners']:
        item['allocation_stack'] = events[item['allocation_event']].frames
    arms[arm] = data
    print(arm, data['counts'], 'RSS', data['solver_peak_rss_kib'],
          'virtual peak', data['peak']['traced_virtual_extent_bytes'],
          'final heap', data['final']['heap_extent_bytes'])
for p in (Path(__file__), root / 'tools/bench/memory_syscalls.py',
          root / 'tools/bench/test_memory_syscalls.py'):
    bind(p)
result = dict(hashes=hashes, arms=arms,
    decision='Exact Release RSS reproduction makes the external trace usable. Candidate retained heap and later clause-arena growth are the next lead; externally observe process-specific RSS/smaps before assigning the resident peak or editing allocation policy.',
    limitation='The ledger covers traced virtual extents, not all process address space, live malloc bytes, or RSS. Linux page rounding4096. Both traces retain27 incomplete loader stacks. The high-water wall-time location is unknown. Tracer timings are diagnostic only; pending proofs remain pending.')
if '--seal' in sys.argv:
    with target.open('x') as out:
        json.dump(result, out, indent=2)
    print(target)

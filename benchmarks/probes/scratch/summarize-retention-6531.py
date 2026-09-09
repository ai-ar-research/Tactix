#!/usr/bin/env python3
"""Bind reviewed machine-code ranges to existing sample-period weights."""
import json
from pathlib import Path
import re
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from perf_annotation import parse_periods

campaign = root / 'benchmarks/runs/20260909-6530-rank32-retention-annotation'
target = campaign / 'summary-6531.json'
assert not target.exists()
manifest = json.loads((campaign / 'manifest.json').read_text())
parent = root / manifest['parent']
hashes = {str((campaign / 'manifest.json').relative_to(root)): sha(campaign / 'manifest.json')}
for rel, digest in manifest['authorities'].items():
    assert sha(parent / rel) == digest, rel
for name, output in manifest['outputs'].items():
    for suffix, key in (('.txt', 'stdout_sha256'), ('.stderr', 'stderr_sha256')):
        p = campaign / (name + suffix)
        assert sha(p) == output[key], p
        hashes[str(p.relative_to(root))] = sha(p)
assert not any(p.is_dir() for p in campaign.iterdir())

global_periods = {}
for task, arm in enumerate(('candidate', 'kissat')):
    p = parent / f'profiles/{task:03}-self.txt'
    data = p.read_text()
    assert '# Total Lost Samples: 0\n' in data
    values = re.findall(r'^# Event count \(approx\.\): (\d+)$', data, re.M)
    assert len(values) == 1
    global_periods[arm] = int(values[0])
    hashes[str(p.relative_to(root))] = sha(p)

symbols = {
    'candidate-rebuild': 'tactix::kernel_rebuild_large_watches(tactix::Kernel&, bool)',
    'candidate-collect': 'tactix::kernel_collect_garbage(tactix::Kernel&, unsigned int, bool, bool)',
    'kissat-rebuild': 'kissat_watch_large_clauses',
    'kissat-flush': 'kissat_flush_large_watches',
}
functions, parsed = {}, {}
for name, symbol in symbols.items():
    a = parsed[name] = parse_periods((campaign / (name + '-periods.txt')).read_text(), symbol)
    functions[name] = dict(symbol=symbol, instruction_rows=len(a.instructions),
        samples=a.samples, period=a.total_period,
        percent_program=100 * a.total_period / global_periods[name.split('-')[0]])

# Bounds reviewed against the exact annotated disassembly. These addresses
# are attribution metadata, never solver controls. All intervals are half-open.
ranges = {
    'rebuild_retention_and_list_bookkeeping': ('candidate-rebuild',
        [(0x410c1d, 0x410cd4), (0x4110e8, 0x4110f2)]),
    'rebuild_entry_loop_subset': ('candidate-rebuild', [(0x410c68, 0x410c97)]),
    'collection_pure_retention_and_metadata': ('candidate-collect',
        [(0x420153, 0x4201db), (0x420780, 0x42078a)]),
    'collection_entry_loop_subset': ('candidate-collect', [(0x420180, 0x4201b2)]),
    'collection_shared_ref_mapping_loop_not_fast_path': ('candidate-collect',
        [(0x4201f0, 0x420203)]),
}
regions = {}
for name, (function, intervals) in ranges.items():
    a = parsed[function]
    assert all(left[1] <= right[0] for left, right in zip(sorted(intervals), sorted(intervals)[1:]))
    period = sum(a.range_period(begin, end) for begin, end in intervals)
    regions[name] = dict(function=function,
        ranges=[[hex(begin), hex(end)] for begin, end in intervals], period=period,
        percent_function=100 * period / a.total_period,
        percent_program=100 * period / global_periods['candidate'])
selected = ('rebuild_retention_and_list_bookkeeping', 'collection_pure_retention_and_metadata')
combined = sum(regions[name]['percent_program'] for name in selected)
for rel in ('tools/bench/perf_annotation.py', 'tools/bench/test_perf_annotation.py',
            'benchmarks/probes/scratch/summarize-retention-6531.py'):
    hashes[rel] = sha(root / rel)
with target.open('x') as out:
    json.dump(dict(hashes=hashes, global_event_periods=global_periods,
        functions=functions, regions=regions, combined_two_retention_sites_percent_program=combined,
        decision='Deprioritize the scan-only proposal as the main Rank32 gap-closing step. Keep baseline contract tests; no implementation or new timing is selected by this attribution.',
        limitations='One sampled pair; instruction skid and frequency-weighted sampling apply. Region shares are not time savings, independent ablations or hardware-stall causes. Mixed-list and bookkeeping work remains after a fast path. Drop-large caller coverage is not measured here; do not claim a complete three-site ceiling. Native scopes differ. Source equality does not establish new linked-image cost.'), out, indent=2)
    out.write('\n')
print(json.dumps(dict(functions=functions, regions=regions,
                     combined_two_retention_sites_percent_program=combined), indent=2))

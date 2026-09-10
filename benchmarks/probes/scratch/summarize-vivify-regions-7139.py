#!/usr/bin/env python3
"""Conserve source-reviewed self-PC regions without claiming instruction latency."""
import json
from pathlib import Path
import statistics
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from perf_annotation import Annotation, Instruction
p = root / 'benchmarks/runs/20260910-7138-vivify-schedule-pc-localization/summary.json'
r = json.loads(p.read_text())
hashes = {str(p.relative_to(root)): sha(p)}
for rel, digest in r['hashes'].items():
    assert sha(root / rel) == digest, rel
    hashes[rel] = digest
target = p.parent / 'reviewed-regions-7139.json'
assert not target.exists()
# Half-open exact instruction intervals, reviewed against frozen source and
# disassembly. These are partial self regions, not complete helper envelopes.
regions = {
    'occurrence_increment_and_touched_fast_path': [(0x4d85b0, 0x4d85fb)],
    'live_large_population_scan': [(0x4d832c, 0x4d838b)],
    'irredundant_regime_recomputed_in_comparator': [(0x4d6cf3, 0x4d6d21), (0x4d6d64, 0x4d6d92)],
    'candidate_partition_swap': [(0x4d6c2e, 0x4d6c86)],
    'literal_snapshot_append_fast_path': [(0x4cc640, 0x4cc66a)],
    'native_occurrence_increment': [(0x445d50, 0x445d73)],
}
arms = {}
for label, arm in r['arms'].items():
    values = {}
    for name, intervals in regions.items():
        if name.startswith('native_') != (label == 'native'): continue
        value = 0; evidence = []
        for begin, end in intervals:
            found = [item for item in arm['symbols'].values()
                if item['address'] <= begin < end <= item['address'] + item['size']]
            assert len(found) == 1
            item = found[0]
            annotation = Annotation(name, 1, tuple(Instruction(row['pc'], row['period'], row['instruction']) for row in item['instruction_rows']))
            subtotal = annotation.range_period(begin, end)
            value += subtotal
            evidence.append(dict(begin=hex(begin), end=hex(end), self_period=subtotal, assembly=item['assembly']))
        values[name] = dict(self_period=value, regions=evidence)
    arms[label] = values
medians = {name: statistics.median(arms[f'candidate-wave{i}'][name]['self_period'] for i in range(3))
           for name in regions if not name.startswith('native_')}
for rel in (str(Path(__file__).relative_to(root)), 'tools/bench/perf_annotation.py',
            'tools/bench/test_perf_annotation.py',
            '.claude/worktrees/runtime-collect-retry-7030/src/rules/vivify/vivify.cpp',
            '.claude/worktrees/runtime-bve-import-bounds-7116/src/rules/vivify/vivify.cpp', 'kissat/src/vivify.c'):
    hashes[rel] = sha(root / rel)
with target.open('x') as stream:
    json.dump(dict(hashes=hashes, arms=arms, candidate_medians=medians,
        interpretation='The two hot comparator regions re-read and recompute the live-irredundant/10 versus live-redundant regime for each comparison. Native pure irredundant scheduling instead uses stable radix count keys. This strengthens the algorithm/representation specialization lead; it does not prove a key-only copy removal or small boolean hoist meets the proposed whole-Vivify/runtime gates.',
        limitation='Regions cover exact self instruction PCs only; no callees, allocation slow paths or complete comparator invocation counts. Skid can place samples after a costly load. Necessary occurrence counting exists in both solvers. Do not equate a sampled instruction weight with instruction latency, sum unlike scopes, or infer full scheduling parity.'), stream, indent=2)
print(json.dumps(dict(candidate_medians=medians, native=arms['native']), indent=2))

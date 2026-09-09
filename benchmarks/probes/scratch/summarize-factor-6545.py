#!/usr/bin/env python3
"""Freeze reviewed Factor instruction regions; no timing-gain inference."""
import json
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from perf_annotation import parse_periods
c = root / 'benchmarks/runs/20260909-6544-rank32-factor-annotation'
target = c / 'summary-6545.json'
assert not target.exists()
m = json.loads((c / 'manifest.json').read_text())
parent = root / m['parent']
hashes = {str((c / 'manifest.json').relative_to(root)): sha(c / 'manifest.json')}
for rel, digest in m['authorities'].items(): assert sha(parent / rel) == digest, rel
for label, output in m['outputs'].items():
    for suffix, key in (('.txt', 'stdout_sha256'), ('.stderr', 'stderr_sha256')):
        p = c / (label + suffix)
        assert sha(p) == output[key], p
        hashes[str(p.relative_to(root))] = sha(p)
assert not any(p.is_dir() for p in c.iterdir())
symbols = {
    'apply': 'tactix::factor_apply_seeded(tactix::Kernel&, unsigned int)',
    'index': 'tactix::(anonymous namespace)::build_factor_index(tactix::Kernel const&, tactix::(anonymous namespace)::FactorIndex&, unsigned int)',
}
parsed = {n: parse_periods((c / ('candidate-' + n + '-periods.txt')).read_text(), s)
          for n, s in symbols.items()}
denominator = 1226065909430
assert f'# Event count (approx.): {denominator}\n' in (parent / 'profiles/000-self.txt').read_text()
regions = {}
for name, begin, end in (
    ('large_match_watch_header_and_reserved_filter', 0x47faa0, 0x47fae6),
    ('large_match_literal_mark_loop', 0x47fb20, 0x47fb5a),
    ('touched_cofactor_count_clear', 0x480470, 0x480482),
    ('binary_count_push_call_and_continuation', 0x481075, 0x481099),
):
    period = parsed['apply'].range_period(begin, end)
    regions[name] = dict(begin=hex(begin), end=hex(end), period=period,
                         percent_program=100 * period / denominator)
for rel in ('tools/bench/perf_annotation.py', 'tools/bench/test_perf_annotation.py',
            'benchmarks/probes/scratch/summarize-factor-6545.py',
            'tools/solvers/variants/kissat-macro-heap-6090/src/factor.c'):
    hashes[rel] = sha(root / rel)
result = dict(hashes=hashes, global_event_periods=denominator,
    functions={n: dict(samples=a.samples, period=a.total_period,
                       percent_program=100*a.total_period/denominator)
               for n,a in parsed.items()}, regions=regions,
    decision='No new remainder-pool or per-literal-index rewrite selected from this profile. Cost is distributed; hottest matching follows native occurrence/mark logic. Prioritize the demonstrated Rank50 admission omission via a read-only producer/consumer lifecycle audit before an approved repair.',
    tests='No parser, solver or strategy change. Reuse the seven strict period-reader tests and exact immutable source/profile/pin hashes. Ranges reviewed against annotated machine instructions; new unit tests would not establish causal performance.',
    limitations='One sampled profile, approximate event denominator, skid, call-graph truncation and out-of-line instruction attribution apply. Exact source equality to current6424 is not a new linked-image timing claim. No elapsed-time savings, heap-allocation counts, complete native work identity or proof acceptance inferred.',
)
with target.open('x') as out:
    json.dump(result, out, indent=2)
    out.write('\n')
print(json.dumps(dict(functions=result['functions'], regions=regions), indent=2))

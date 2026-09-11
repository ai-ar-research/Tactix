#!/usr/bin/env python3
"""Freeze the completed solver-phase transfer table and runtime audit findings."""
import json
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
source = root / 'benchmarks/reports/2026-09-10-vivify-revalidation-full50-7168/summary.json'
target = root / 'benchmarks/reports/2026-09-10-runtime-evaluation-audit-7173.md'
assert not target.exists()
s = json.loads(source.read_text()); rows = s['rows']
assert [r['rank'] for r in rows] == list(range(1, 51))
assert all(r['exact_parent_work_and_proof'] for r in rows)
assert all(r['validation'] in ('checked_SAT', 'checked_UNSAT', 'pending_validation') for r in rows)
assert sum(r['expected'] == 'SAT' for r in rows) == 25
candidate = sum(r['candidate_seconds'] for r in rows)
native = sum(r['kissat_seconds'] for r in rows)
parent = sum(r['parent_seconds'] for r in rows)
assert abs(candidate - s['score']['candidate_par2']) < 1e-8
assert abs(native - 7239.010) < 1e-8
checked = sum(r['validation'] != 'pending_validation' for r in rows)
lines = [
    '# Runtime evaluation audit and fresh Rank 1–50 transfer', '',
    'Frozen evidence, 2026-09-10. Solver snapshot: ' + s['observed_at'] + '.', '',
    '## Full-set outcome', '',
    f'All fifty expected outcomes (25 SAT, 25 UNSAT), exact parent7030 non-timing work and certificates; no solver timeout, loss, or published invalidity. Independent checks: {checked}/50 checked-clean at the snapshot; the remaining checks continue asynchronously and do not become accepted merely because fingerprints match.', '',
    f'Tactix {candidate:.3f}s / fixed Kissat {native:.3f}s = {candidate/native:.9f}x. The <=1.01x target still requires {candidate-1.01*native:.4f}s reduction. Parent7030 was {parent:.3f}s: the new cross-window view is {parent-candidate:.3f}s lower ({100*(1-candidate/parent):.4f}%). This is a transfer observation, not causal attribution to the Vivify change; the paired7150/7158 evidence remains its unit-cost authority.', '',
    f'Maximum candidate RSS is {max(r["candidate_peak_rss_kib"] for r in rows)/1048576:.9f}GiB. No observed 16-GiB exceedance or solver OOM; memory competitiveness against Kissat remains unmet. Rank35 RSS is {rows[34]["candidate_peak_rss_kib"]/rows[34]["parent_peak_rss_kib"]:.6f}x parent in this fresh view; the previously accepted local tradeoff is not hidden.', '',
    'Rank21 stays in every score but remains shelved for debugging. Preserve established closures4/32/33/49/46. The largest unshelved absolute gaps are:', '',
    '| Rank | Excess seconds over fixed Kissat | Ratio |', '|---:|---:|---:|']
for r in sorted((r for r in rows if r['rank'] != 21), key=lambda r:r['candidate_seconds']-r['kissat_seconds'], reverse=True)[:10]:
    lines.append(f'| {r["rank"]} | {r["candidate_seconds"]-r["kissat_seconds"]:.3f} | {r["candidate_over_kissat"]:.6f} |')
lines += ['', '## Runtime bookkeeping audit', '',
    'Actual retained7144/fresh7162 Release binary SHA: `cdcb735715d4eb2c23d43bbff14cb9d5ccedaa41592aab693cdc779b531dbdcf`. Build flags are `-O3 -DNDEBUG`; EVENT_TRACE, RULE_JOURNAL, PROOF_SHADOW and CONFLICT_STOP are OFF. Symbol/string inspection agrees: no EventTrace/rule-journal implementation or their trace environment names; `now_nanos`, `TACTIX_GRID_TRACE`, and `TACTIX_FACTOR_TRACE` remain.', '',
    'Concrete optional work remains:', '',
    '- `kernel.cpp:propagate_literal_binary`: tracks mixed-list order and large words, then updates six diagnostic counters. Consumers in this source are the frontend statistics printer, not policy. Fresh Rank35 reports137655446 scans, all flagged binary-first. Therefore diagnostic updates execute137.7 million times, but the mixed-list residual tracking has zero reachability on that instance; do not price the unused branch as Rank35 overhead.',
    '- `registry.cpp:dispatch_rule` and three scheduler paths: two wall-clock reads per timed invocation. Rank35 reports2021962 BVE and3976462 TransitiveReduce applications. Ordinary DefaultPolicy CDCL propagation/analysis use the direct untimed path. Coarse solve/slot timers are separate.',
    '- `factor.cpp`: remainder offered/materialized/reused statistics and runtime trace lookups remain. In saved7171 Factor wave0 the direct getenv child accounts for37641133 sampled periods out of53880322120 inclusive Factor periods, roughly0.070%; this is sampled attribution, not an isolated measurement or an explanation of the whole gap.', '',
    'The unchanged runtime runner clears inherited TACTIX_/KISSAT_/SAT_EVENT_/SAT_INPROCESS_ variables before launching. It enables external perf stat counters when available and samples process-tree VmPeak at100ms intervals. The fixed native campaigns5712 and6108 also enable that context collection; inspected final commands confirm perf stat on both lineages. This is shared harness overhead, not a Tactix-only tracing mismatch. The wall timer includes process/wrapper startup, wait granularity and sampler shutdown, so tiny-instance timings are not intrinsic solver-core costs. Decompression, pin/input hashing, certificate hashing, independent checker/model validation and scratch removal are outside the measured solver invocation. Proof emission and normal statistics printing remain inside.', '',
    '## Decision and bounded next experiment', '',
    'Retain7144. Full solver-phase revalidation is complete, but final proof/cleanup, current-source Claude-owned fuzz and consolidated-production gates remain open. Production c2a95a6 is unchanged. No old campaign or binary was modified.', '',
    '7172 isolates a joint ablation of binary-scan telemetry and individual rule clocks using an OFF-by-default experimental compile flag. Preserve Stats/Kernel layout, public actions, logical tick currencies, scheduling budgets, rule-call counts, proof/model paths and coarse phase/solve timing. Enabled control must reproduce7144 byte-for-byte. Both instrumentation modes need full suites and explicit dispatch/binary-scan tests. B28+C5 and then three independent paired31/35/38 waves require all non-timing statistics except exactly validated c binprop rows and all certificate fingerprints to match. Independent proof checks proceed asynchronously. No minimum percentage is invented to reject a real gain; report paired median/range, RSS and noise, and do not confuse the joint ablation with individual-cost attribution.', '',
    'Larger competing lead remains Factor: saved7171 inclusive costs are53.88–54.04B periods versus32.62B native. Candidate build_factor_index (~10.3B), sparse-watch rebuild (~5.8B) and remainder lookup (~4.4–4.6B) are major children. Function inlining differs, so native entry-self is not a matched unit-cost denominator. The required sparse/dense transitions and already-borrowed binary prefixes must not be mislabeled removable diagnostic work.', '',
    '## Full per-rank table', '',
    'Seconds are proof-on solver wall time; RSS ratio is candidate/native. Status is at the frozen solver snapshot, not a live checker claim.', '',
    '| Rank | Verdict | Tactix s | Kissat s | Time ratio | RSS ratio | Validation |',
    '|---:|:---:|---:|---:|---:|---:|:---|']
for r in rows:
    lines.append(f'| {r["rank"]} | {r["expected"]} | {r["candidate_seconds"]:.3f} | {r["kissat_seconds"]:.3f} | {r["candidate_over_kissat"]:.6f} | {r["candidate_peak_rss_kib"]/r["kissat_peak_rss_kib"]:.6f} | {r["validation"]} |')
lines += ['', '## Provenance and checks', '',
    f'- Full solver snapshot: `{source.relative_to(root)}`, SHA `{sha(source)}`; its sibling comparison.tsv includes instance identities, raw RSS and parent timings.',
    '- Runtime wrapper: `benchmarks/runs/20260910-7167-vivify-revalidation-ranks1-50/pins/runtime_task.py`; native contracts:5712/6108 frozen manifests and final commands.',
    '- Factor attribution: `benchmarks/reports/2026-09-10-factor-envelope-7171.json`; exact-PC localization: `benchmarks/runs/20260910-7170-factor-pc-localization/summary.json`.',
    '- Test consideration: this report transforms the strict7168 table, asserts all50 ranks/expected statuses/work equality/aggregate conservation, and adds no scoring exclusions. New telemetry contract fixtures reject malformed/duplicate/missing/nonzero ablated counters and five semantic changes.', '']
with target.open('x') as stream: stream.write('\n'.join(lines))
print(target)

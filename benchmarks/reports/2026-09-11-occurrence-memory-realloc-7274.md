# Current occurrence memory and isolated realloc gate

Decision checkpoint, 2026-09-11. Retained7144 and productionc2a95a6 are unchanged.

## Complete single-image transfer

[7262 comparison](2026-09-11-occurrence-full50-7262/comparison.tsv) retains all50
approved instances, including shelved21. All expected verdicts and parent work/
certificate fingerprints match under the predeclared7256 comparison: only timing
and the two strictly parsed physical dense-storage counters differ across arms.
The solver snapshot has39/50 independent checks complete; later checks belong to
the live results and7274 audit, not a rewritten snapshot.

Total8057.066s / fixed Kissat7239.010s =1.113006613x. The <=1.01x target remains
unmet by745.6659s. The14.993s decrease from parent8072.059s is a cross-window
observation, not causal speedup. Repeated7261 medians on31/35/38 remain
1.000664/1.002473/1.004903; complete-Eliminate cost fell about5.23%, but this
phase occupies only about8.9% of the whole run and outside offsets remain.

The full transfer corroborates useful memory reduction, not memory parity:

| Rank | Current peak MiB | Kissat peak MiB | Change from parent MiB |
|---:|---:|---:|---:|
|9|2647.30|1295.15|-508.49|
|25|2790.45|867.99|-550.37|
|21 (scored, shelved)|2632.31|1402.73|-472.24|
|31|483.55|259.52|-66.71|
|35|512.54|263.05|-74.04|

All seven RSS increases are retained:4 +0.195MiB,6 +0.973,17 +7.859,
20 +27.465,38 +4.094,49 +0.559,50 +3.121. Rank50's small absolute increase
is7.43%; percentage alone does not rank the largest memory gap. Maximum RSS
remains about15.27GiB. This does not establish hard-limit16-GiB success or
global memory competitiveness.

## Current causal lead and scope

[7265/7266 memory authority](../runs/20260911-7265-occurrence-memory/summary-7266.json)
uses immutable7253 trace binaries with only the existing slot-memory observer.
Ranks9/25 both have exact whole own-Release work/certificates, including physical
counters, independently checked original SAT models and clean final results.
Rank21 is not part of this diagnostic.

On both witnesses the first maximum-RSS sample is BVE capacity-stage end after
the ordered payload capacity changes from536870908 to1073741820bytes. At that
sample Rank9 current RSS is2183576KiB versus maximum2707248KiB; Rank25 is
2334524 versus2858040KiB. Thus the process has already dropped roughly511MiB
below its high-water mark. This is consistent with vector allocate/copy/free
growth, not allocation-level proof: the trace brackets multiple operations,
contains unsampled intervals, and does not inventory every owner.

This current observation, not historical6156, supports testing a realloc-backed
four-byte FlatLists payload. Kissat's local vector growth uses realloc; it can
grow/remap or move, and an improvement is not guaranteed. Retained formula,
reconstruction and other owners remain large. This is a memory repair, not a
claim to explain the remaining11.3% runtime gap.

## Isolated implementation and validation

[7271 build authority](../runs/20260911-7271-dense-realloc-build/manifest.json)
owns source7267, candidate Release/BC SHA
`f9caf175e10e9a875fc6c42b3e2d7b60dc401909573a8ed87f2aa23ce66654e6`.
Only the FlatLists payload owner changes. Three-pointer footprint, capacities,
zero-initialization, offsets, traversal order, packing/defrag policy and counters
remain exact. Allocation failure terminates, never reports SAT. All scheduling,
budgets, proof-rule guards, public actions and reconstruction rules are unchanged.

- Full119/119/118 Release/BC/trace suites pass without skips.
- 35815 deterministic vector operations preserve exact size/capacity/value state;
  forced-moving/null-failure/overflow, tagged values and copy/move are covered.
- Actual7253 parent:272 FlatLists physical boundaries,364 cross-rule boundaries
  across52 cases, and840 replacement cases have exact output.
- Six intentional fatal-path controls terminate with SIGABRT and cores disabled.
- [7268 sanitizers](../runs/20260911-7268-dense-realloc-sanitizers/summary.json):
  ten actual-library fixtures twice, ASan/UBSan/LSan outside ptrace, no suppression.
- [7269 proofs](../runs/20260911-7269-dense-realloc-proofs/summary.json):156 checked
  UNSAT proofs,156 original-formula SAT models, two rejecting checker controls;
  exact parent certificates. Only63972bytes of owned fixed CNF/proofs removed.

Initial7267 validation failed because the copied source lacked the green parent's
tools/benchmarks/plan support links (22 harness failures and two skips per suite).
Restoring those links yielded green suites.7270 then failed only because its
sealer expected older CTest success wording; installed CTest4.4 prints a different
success sentence.7271 retains both failures and verifies unchanged compiled
binary/object hashes, then runs all suites, fixtures and parent comparisons.
No solver or test acceptance criterion was changed to pass either failure.

## Next gate, bookkeeping and limitations

7272 B28+C5 compares actual7253 with7267, all66 tasks fullwidth under the unchanged
12CPU/48000MiB/1200s solver-only/16000s checker/5h contract. Its inherited display
job name is7256-occurrence-BC; job1681013 and the7272 launch/path/hash authorities
unambiguously identify the new isolated campaign. All66 solver outcomes and33
pair fingerprints passed;64 independent checks were complete at runtime launch.
Existing jobs are untouched.

After all66 exact solver outcomes,7273 measures three independent paired waves
on9/25/31/35/38 (ten tasks each, middle order reversed). Preserve every non-timing
counter, including dense moves/defrags; no physical-counter exclusion is allowed
for this allocator-only change. Judge median/full range and peak RSS; require
<=2% within-arm spread for absolute timing claims. Do not wait for proofs to
continue provisionally, or turn pending checks into acceptance.
Wave0 job1681079 was submitted at05:07:18EDT while the last two BC proofs checked.

The user-requested current bookkeeping audit confirmed Release event tracing,
journaling and proof shadow OFF, but binary-scan diagnostics, per-rule clocks
and some disabled-trace environment lookups remain. Earlier7184 joint ablation
showed no consistent net gain. The current change deliberately retains telemetry;
logical effort counters remain scheduling inputs, not disposable diagnostics.
Proof generation is timed; independent checking, decompression and cleanup are
not. Perf stat/memory sampling match the fixed native runner contract.

Full50 allocator transfer, independent checks/residue, memory parity, current-source
Claude-owned fuzz and consolidation remain open. No denied fuzz-artifact access,
transfer or push is retried. Codex debugs directly; no Claude debugging delegation.

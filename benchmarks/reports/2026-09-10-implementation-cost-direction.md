# Implementation cost before more trace alignment

The user's priority is general CDCL/inprocessing efficiency and a scheduling
mechanism at least as strong as Kissat's. The existing work-versus-unit-cost
contract in DEVELOPMENT_PLAN remains applicable; no solver default or gate
changes here. The preceding status-only turn was no progress. This increment
validates the pending Rank31 witness and derives a new cost discriminator.

## Rank35: most sampled excess is inside inprocessing

[7079](2026-09-10-rank35-cost-scopes-7079.json) binds the repaired7030 profile
and cached native profile, now both independently checked UNSAT and clean.
Their own full Release work/certificate replay was validated previously.
It reads exact flat symbol rows, not repeated call-tree renderings, and scales
each rounded percentage by its own profile's sampled cycle-period total.

| Function scope | Tactix billion cycles | Kissat billion cycles | Difference |
|---|---:|---:|---:|
| Complete sampled program |1352.636|1127.748|224.888|
| Inprocessing entry functions |482.079|304.154|177.926|
| Ordinary propagation, all callers |477.751|502.073|-24.322|
| Reduce and its collection children |58.028|26.728|31.300|
| Vivify, including scheduling/transitions |141.621|115.707|25.914|
| Factor |53.970|32.592|21.378|
| Congruence |43.690|20.751|22.940|

These are approximate sampled cycle totals, **not measured unit costs or
replicated acceptance timings**. Nested rows must not be summed. Inprocessing
includes Tactix `run_phase` versus native `kissat_probe`, `kissat_probe_initially`
and `kissat_eliminate`; the three native functions are separate entry paths.
Whole function scopes include root preparation on both sides, unlike the
previously incomparable printed timers. Initial native probing is explicitly
included; whole-symbol Congruence also includes its initial calls.

About79.1% of the sampled total excess falls within this inprocessing envelope.
This changes the next action: investigate its broad implementation overhead,
with Reduce/collection as a second cost area. Ordinary propagation is hottest
but is not shown to have excess total cycles on this witness. It may still
have local inefficiencies; the evidence does not establish universal CDCL
parity.27 matching major counters are not full formula, effort or pose equality.
Sampling, unwinding and separate hardware windows remain uncertainty sources;
the JSON's rounding bounds do not quantify those uncertainties.

## Source-level discriminator and scheduling floor

Current source retains separate Probe/Eliminate clocks and phase-owned
transitions. Kissat `probe.c` and `eliminate.c` confirm these are different
pipelines, not interchangeable timers. Tactix builds a separate dense occurrence
image and reconstructs logical arena order; its Vivify schedule keeps pass-time
literal snapshots. Kissat's Vivify uses physical clauses; CaDiCaL also separates
candidate references/count keys from clause state. These representation costs
are hypotheses to measure, not permission to erase ordering/proof contracts.
The rejected6822 snapshot-copy and6902 binary-reconnect changes stay rejected.

Next measure common entry-to-exit scopes and their actual work: candidate
admission/preparation, gate/embedded solving, rewrite/proof/reconstruction and
occurrence/watch maintenance. Start with the whole Eliminate/BVE pipeline and
Vivify scheduling, not just a small leaf.7079's BVE attempt-function comparison
is intentionally not priced as equivalent work: Tactix's function includes
mutation that native `kissat_generate_resolvents` does not. Resolve that scope
boundary before assigning its apparent41.465-billion-cycle difference to a
specific implementation. Preserve all adverse controls and major closures.

Audit scheduling at common states alongside cost: triggers, material/retry
producers, candidate order, effort currency/anchor, overshoot, bound ramps and
termination. Matching constants alone is not N4 verification. No new broad
scheduling-parity claim or instance-specific tuning is justified here.

## Rank31: admission, not an arbitrary heap tie

[7075](../runs/20260910-7073-rank31-bve-priority/solver-summary-7075.json)
passes exact full own Release work/certificate and every7068 macro, boundary
and candidate byte. At the selected phase's initial heap,355342 has identical
9-positive/8-negative occurrence counts, but is absent in7030 and present at
position7319 in6730. Initial heap sizes are18651/18652. Tactix's `insert_initial`
filters on active state and `bve_retry`, mirroring native active/eliminate flags.
This is an incoming admission difference, not evidence for changing tie logic.
The recorded zero score on an absent candidate is not its computed priority.
It later enters and declines, whereas the predecessor eliminates it. The exact
producer history and native disposition remain unproven; no compensating retry
is restored. The selected budget is not exhausted. Independent checks continue.

Rank43 diagnostic7077 remains prepared, **unsubmitted**, secondary to this
implementation-cost investigation. No new solver execution, source edit,
promotion, fuzz access, denied transfer/push retry or FlatLists redesign.
Seven new profile-reader tests cover tree double counting, whitespace, exact
symbols, report kinds, malformed/duplicate/missing data and rounding. Five
native-statistics tests belong to the preserved7077 preparation. No solver
unit-test change is required for this read-only diagnostic increment.
7080 owns live campaign/result/test/cleanup observations; Andrew's successor
TeX/PDF is local and unsent. Existing documentation-size debt is not compacted.

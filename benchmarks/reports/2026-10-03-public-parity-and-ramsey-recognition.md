# Public parity action and structural Ramsey recognition checkpoint

2026-10-03. Public parity implementation is present in the worktree; production
acceptance remains OPEN. Ramsey recognition is an isolated research component,
not an installed rule or a newly closed target.

## Shared public parity action

`ParityConsequence` is registered append-only as rule 29 (calculus X8), with C
parameter tag 31. The typed profile bounds clause/group collection, elimination
work, intermediate/source/output widths, proof additions and helper extent;
it also selects pivot direction and consequence ordinal. Zero limits disable
work; no production allowance was tuned. The canonical action has no additional
integer instance payload. Public C/C++, configured DefaultPolicy and the random
controller share the same scoped parameters, serialization and execution.

The existing live adapter checks complete XOR encodings in the current formula,
selects a bounded certified GF(2) sum, preflights the proof, reserves monotone
proof-only helper identities, emits and retires the helper definitions, then
publishes original-variable consequences into continuing CDCL. This is reusable
Boolean reasoning, not a model constructor. Helpers are not branchable semantic
extension variables; support for such definitions is still separate work.

A borrowed structural-provenance stream is required. Binding invalidates stale
state selections without resetting action IDs. The CLI exclusively creates
`--structural-provenance=NEW_FILE`, preserving existing files and rejecting
input/proof/feedback aliases. Records include action context, source and mapping
information, proof additions/deletions and completion counters. Nine raw parity
statistics distinguish attempts, scanned clauses, equations, selection work,
preflight additions, emitted additions/deletions, helpers and publications.
Reward and training remain deferred. Observation is read-only.

The guard requires propagated, consistent sparse root state, no pending conflict
or probe transaction, connected watches, a provenance stream and an available
action ID. LRAT/PBP decline before proof mutation. The reference default program
omits X8 pending an accepted scheduling profile. Explicit configured execution is
available; this temporary omission is recorded as open N4 debt in
`SP-PARITY-CONSEQUENCE-1`, not as an accepted parity exception.

## Checked evidence and failures retained

Evidence root: `benchmarks/runs/20261003-parity-public-action/`.

- Sixteen of seventeen focused CTests initially passed. `random_online` failed
  because its all-registry fixture had no required provenance stream. The fixture
  was repaired; its dedicated rerun passes. Both logs remain. This is not a claim
  that the complete suite has passed on the final pin.
- The registered-action fixture exercises typed C/C++ execution, insufficient
  preflight allowance without namespace/proof mutation, productive equivalence,
  duplicate suppression, pure observation, raw after-action feedback and strict
  replay. Its native proof is 7,033 bytes and has SHA-256
  `35d3e02c8022517a14fb02f8b1fcc876b3e50cb64774bac46634a3d678ab2fab`.
- Canonical witness-free DSR conversion/checking, forward DRAT checking and
  independent every-addition LRAT/CakeLPR checking pass. The closing gadget's
  premise IDs are excluded from all prefix hints. A false unit is rejected by
  the oracle, all helper clauses are retired, and both published equivalence
  clauses remain. `check-result.json` binds commands, checkers and proof bytes.
- The final proof-population-enabled Debug pin passes the same direct fixed test
  and emits the byte-identical checked proof. This is scoped microtest evidence.
- Actual CLI random-controller record/replay publishes two clauses per run and
  passes original-input SAT model checking; proof/provenance bytes agree. The
  existing both-policy frontend purity/replay test also passes (two proofs and
  two models). `frontend-result.json` and the frontend log own those outcomes.
- Early test setup failures are retained: stale rule/stat counts, missing stream
  binding, and an unbounded test action profile were corrected. A CLI seed that
  resumed search before applying X8 remains as a zero-coverage control; seed 2
  exercises the fixed productive scenario. This is not a performance experiment
  or a substitute for fresh policy fuzzing.

## Acceptance jobs: submitted, not yet accepted

Both pins bind 821 source/support files from the current worktree based on
`6fe14539`, including earlier uncommitted graph/guidance work. HEAD alone is not
sufficient to reproduce the binaries. Debug additionally binds 187 executables.
The full Debug preflight verified 1,015 binary/source/support bindings. An initial
freeze attempt detected a source change during compilation; it was refused and
repaired by an explicit incremental rebuild and matching final source hashes.

| Scope | Job | Candidate |
|---|---|---|
| B28+C5 preservation | 1747647 | Release pin |
| Rank1–50 preservation | 1747662 | Release pin |
| WV100 preservation | 1747663 | Release pin |
| WV2–89 preservation | 1747665 | Release pin |
| Full 225-test Debug | 1747703 | Debug pin |

Release binary SHA-256:
`8760cd4d1bfcd2ba0d27efbe0abf66acc072200b9be5d24fba9c8a95f98b96d6`.
Debug binary SHA-256:
`e5719daa9e0d52ab6df80be30a9fd4c6aed46a09bc521709d43fba01993249c2`.
Pins are `build/pin-parity-public-{release,debug}-6fe14539-01`.
At collection, B28+C5/Rank had completed and running tasks; WV/WV2 and full Debug
were pending. Maintained scoring and artifact validation are still required.
Arrays use the original resource contracts and full width; proof checking is
bounded at 1,800 seconds, with the established slowdown allowance
`max(0.3 * reference_seconds, 20 seconds)`. Censoring is not verification.
These runs preserve the reference default configuration; they cannot accept a
future configuration that enables X8.

Fresh Claude-owned fuzz was launched from
`benchmarks/runs/20261003-parity-public-fuzz-handoff/REQUEST.md`.
The request specifies at least 100,000 fresh cases per policy, the thirteen-group
controller matrix, productive configured-default and actual random-root X8
coverage, repeated application, proof/model oracles, interruption, provenance,
replay and purity. Only the designated public owner report or bounded failure
handoff is consumed by Codex. No terminal report exists at this checkpoint;
launch is not acceptance. Earlier graph/guidance fuzz predates parity changes.

## Ramsey admission independent of variable numbering

The checked standalone Ramsey derivation previously reconstructed edges using
fixed numeric identities. The new isolated recognizer recovers an exact complete
signed K4 encoding after arbitrary edge-variable renaming, sparse external
identities, per-variable complementation and clause/literal reordering. It does
not use a filename or target rank. It does not yet emit a proof or run in Tactix.

For a complete graph on n vertices, two edge variables sharing an endpoint occur
together in `2(n-3)` clauses of the two-color K4 encoding; disjoint edges occur
together in two. Pair frequencies therefore recover the line graph. For adjacent
edges `(a,b)` and `(a,c)`, common neighbors consist of the `n-3` other edges at a,
plus the isolated edge `(b,c)`. For n at least six, the former form a clique of
at least two vertices, distinguishing the star. Deduplicated stars recover graph
vertices, and each Boolean edge must belong to exactly two recovered stars.

Co-occurring signs recover each variable's color orientation, up to global color
exchange. **These structural statistics only propose a mapping.** Admission then
reconstructs every positive and negative K4 clause and compares against the exact
input clause set. The expected clause count, distinct identities and unique
occurrences ensure there are no missing or extra premises. Every recovered K4
records the input occurrence IDs for its two clauses. The recognizer rejects
duplicates and mixed extra constraints; these are explicit initial engineering
restrictions, not claims of impossibility for more general formulas.

Fixed deterministic controls cover n=6,9,18,19 with sixteen remappings each:
64 recognitions pass exact independent source/mapping checks; 384 malformed,
missing, sign-corrupted or duplicate controls decline; 144 effort/resource-bound
controls pass, including exact-work acceptance and one-unit-short exhaustion.
Maximum charged work in this fixed set is 1,953,692. Work counts declared loop
operations; it is not a wall-clock or full allocator-cost model. Parameters in
this research test are test allowances, not proposed production defaults.
Artifacts and compiler/test/source/binary bindings are under
`benchmarks/runs/20261003-ramsey-structural-recognition/`; the editable research
copy lives under `benchmarks/probes/scratch/ramsey-recognition/`.

Before migration, normalize SR witness domains to positive external variables:
`Proof::plan_dsr_witness` requires positive sources. A signed recovered edge must
not be copied blindly into a witness source. Translate the complete derivation
under the recovered signed bijection and independently check renamed certificates.
The startup-only original-database boundary remains necessary for the symmetry
prefix; this work grants no restart-time SR permission.

## Replacement objective and next work

The plan now explicitly requires eventual removal of dedicated LymphoSAT local
search/constructors from the active solver, recovery with those paths disabled,
and only then production removal after certification, fuzz and regression gates.
Retire/version public choices explicitly without unrelated renumbering or silent
replay redirection. Generic Kissat-style Walk remains available.

A read-only census of saved evidence identifies 34 sole LymphoSAT winners in
WV100 (24 SAT, 10 UNSAT), 25 fastest LymphoSAT entries in WV2 (11 SAT, 14 UNSAT),
and 20 LymphoSAT-checked fixed-miss entries (11 SAT, 9 UNSAT). These are 77 distinct
instances across 79 memberships. WV2 fastest ownership is not sole ownership;
none of these labels attributes an upstream internal technique. The bound census
is `lymphosat-replacement-targets.json` in the parity evidence directory.

Next collect and score live gates, disposition fresh owner fuzz, and complete X8
scheduling/relevant-change work. Migrate the bounded Ramsey recognizer and compact
startup derivation through shared typed execution, preflight, provenance and
feedback; extract shared BDD facilities rather than duplicate them. Continue
compact graph interval/counting and the other structural replacements. No new
portfolio target, default performance gain, N1–N4 acceptance, constructor removal
or completion of the 44-target checkpoint is claimed. No baseline/full comparison
was rerun. All active campaign and proof evidence is retained; no cleanup deletion
or push was performed at this checkpoint.

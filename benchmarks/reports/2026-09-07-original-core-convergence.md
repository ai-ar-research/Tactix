# Original-core and binary-order convergence

The objective remains Rank 1–40 at most 1.01x Kissat, with checked B28+C5
regression gates and general implementation repairs. It is not achieved.
This continues the [runtime review](2026-09-07-runtime-review.md) from local
commit `c2a95a6c79c21a76516cf556b6538e76da369c34`. Its push to shared
`origin/main` was rejected by auto-review pending explicit user approval;
no retry or alternate publication was attempted.

## Current gate evidence

5314/job 1668472 has all 40 paired solver results: final-r2 Tactix
8,643.178 seconds / pristine Kissat 6,759.074 = **1.2788x**, provisional
until certificate checking completes. At this snapshot 77/80 records are
checked/clean, zero final invalid. This contradicts any whole-suite gain from
the previous Rank-34-only improvement. 5315/job 1668473 is terminal with all
66/66 records checked and clean. Final-r2/starting time is
1,116.777/1,095.991 = 1.018965x; this closes the cumulative B28+C5 guard, not
the Rank-40 goal. Exact tasks 64/65 are COMPLETED/0:0. Both campaigns retain their
exact immutable source/binary identities; do not repin them to a later repair.

## Why another core adapter, and what is not repeated

The historical report sections 3.44–3.66 include several retracted explanations.
The useful pieces are the actual shallow core-antecedent graph, save/add
eligibility split, and native one-step original-unit justification. Rejected
approaches include a DFS over the transitive LRAT cone and unconditional
root propagation between core units. The soundness root cause eventually
identified there was speculative replay on an unpropagated root, not an
unentailed original clause. These distinctions are retained.

5316/job 1668618 isolates metadata only on current `c2a95a6`: it restores the
relevant Tactix metadata from `9a4adfa..d42c71d`, without that old patch's
search changes. Root-derived units record their immediate reason first and
then direct root antecedents in physical literal order, independently of the
transitive LRAT chain. `core-surface-ordered.cpp` verifies all five ordered
core rows and original/learned tags against pristine Kitten on a fixed
three-clause input. The SubSolver unit suite passes.

Both semantic event traces remain byte-identical to 5313. Across all 98 shared
core-producing queries at seed 563, ordered clause identities and original/
learned tags match exactly. The observer's learned clauses initially use their
creation pose, whereas Kitten prints live watch pose; this is not a different
clause identity or graph. At query 20 both recorded core streams expose
`-970,-693,-693`; the saved original is now before the target lemma. Tactix
still asks its two extra queries because this observer commits nothing.

## Isolated semantic candidate

5317/job 1668620 owns an explicitly **unfuzzed** candidate, not production:

- Preserve core antecedents separately from proof hints and retrieve core
  literals in live arena pose with one id-to-output-slot join.
- Filter original eligibility at `sweep_capture_dir`, i.e. the SAVE boundary,
  before the reverse query or any subsequent core-unit commit changes roots.
- Visit saved core clauses in that order; canonical application commits each
  justified unit immediately, without intervening propagation. Pure detection
  remains read-only and evaluates actual host roots, not promised assignments.
- Learned units keep their independently justified proof prefix. Original
  units get a one-step proof naming the original; the existing general planner
  prepends host root-unit hints. Do not copy the entire core into each proof.
- Only the speculative unit fallback drains a pending root before replay;
  propagate any resulting UNSAT status. No formula/rank/size policy gate exists.

Fixed `core-capture-proof.cpp` tests the actual save/capture/emission integration:
an original with multiple non-false literals is excluded at SAVE; after host
roots are fixed, it is retained and emits native LRAT
`6 -1 0 2 3 4 1 0`. The independent checker verifies it, and the pending
propagation cursor is unchanged. Both `test_sweep` and `test_subsolver` pass;
44 emitted derivations verify through drat-trim and lrat-check.

The exact Rank-34 boundary now closes: all 696 ordered input clauses and 141
registered variables match; all 112 queries, tick deltas and returned models
match, with the two extra trivial directions gone. All 98 captured cores match
in order, original/learned tags, and full literal pose. All 94 emitted units
at conflict 26,913 match in order, including
`-841,-970,-693,-653,-651,-618,-844`. The complete environment sequence also
matches. Both stopped jobs delete scratch. These are causal boundary checks,
not proof/model acceptance or runtime evidence for a complete solve.

## Checked runtime and residual root order

5318/job 1668623 runs a clean Release copy without observer instrumentation:
`d6701f2e4c158b056f13956c4f02acf12b38c5c81805757c6066bdd56ef4b72a`.
All six proof-on tasks finish model-checked and scratch-clean. Single-wave
candidate/current ratios: Rank 17 **0.66789** (164.528/246.340 seconds), Rank 21
**1.01130** (112.198/110.944), Rank 34 **1.40836** (788.751/560.049).
Rank 17 propagations fall 1.921B to 1.219B, but Rank 34 rises 1.323B to 1.843B.
Rank 21's three reported work counters match; instructions rise by 0.892%.
This is diagnostic, not decision-grade acceptance evidence. The core-only
repair is not promoted or sent to broad gates on the strength of Rank 17.
The parent-ID watcher reports FAILED because its accounting response contains
out-of-matrix rows; an exact query of tasks 0–5 confirms six COMPLETED/0:0
records with the correct name and submission time. Checked artifacts govern.
5319/job 1668629 reuses the immutable 5317 trace pins and is terminal/clean.
All 77,627 ordinary decisions in its 50,000–75,451 window match; warm-up still
diverges at conflict 75,451, now level 24 (`-2733` versus `14194`) rather than
the former level 33. Explicit core-unit order alone does not close the heap
tie. 5320's attempt to select every root through the old selected-variable
observer is invalid: Tactix fills its cap with unrelated Substitute maps and
Kissat's fixed-size selector does not cover the full variable list. Both jobs
stop and clean up; these traces establish no root-order claim. Corrected
all-roots campaign 5321/job 1668633 is terminal/clean:
the first 68 root literals match; then Tactix propagates `-583,-587,-581,...`
from binary owner `562`, whereas Kissat gives `-587,-583,-1038,...,-581`.
The observer also contains one additional `-1278` record, which may be a
speculative root operation, not an extra committed unit.

5322/job 1668635 is an isolated representation ablation: after multiset
reconciliation, publish the ledger's binary order to physical watch words
without moving large watches or changing membership/proof identities. Three
focused tests and 44 proof cases pass, but both root traces remain byte-identical
to 5321. Therefore the hypothesis that publication at that boundary closes this
witness by itself is rejected; it is not independently supported for promotion.
5323/job 1668637 sees the same initial `562` list (`-841`) through the preceding
Vivify; it first separates during Sweep. Its mid-propagation watch snapshots
are not authoritative because the scanned physical list is being compacted;
only stable boundaries are used. 5324/job 1668639 identifies an intermediate
Kissat large-to-binary collapse to `(562,-587)` from
`(-1038,562,1035,972)`, absent from Tactix's combined rewrite.

5325/job 1668641 applies the already-certified pairs separately in emission
order after all proof emission. It restores that intermediate collapse, but
root traces remain byte-identical to 5321. It is not a standalone root-order
repair. 5326/job 1668643 tests the interaction with physical binary-order
publication: the first mismatch moves from root record 68 to 70. The first
two owner-562 roots now match (`-587,-583`), but `-581` is still before
`-1038,-1110,-690,-616` instead of after them. Thus neither ablation works
alone, but their combination partially closes this witness. Both pass three
focused tests and all 44 proof cases. Release SHA is
`7128bacabd13ef28d593e0e880b497cb2befd3a44ef2be0a08d6d43d27fae6d4`.

5327/job 1668645 tests a bounded pose-only ablation: tag newly emitted pairs,
then move the last extant copy of each directed bridge to the end of its
current mixed stream just before that pair's rewrite. Known pairs are left
alone. No membership/proof identity changes, and absent images remain absent.
All proof emission still precedes all rewrites, preserving captured source
lifetimes. Physical copies have no separate identity: an earlier rewrite may
have changed a future bridge, so this is not yet a general reconstruction of
reference birth order. The duplicate/absent-image fixture, three focused
suites, and 44 DRAT/LRAT cases pass. Both stopped tasks are terminal and clean.
The owner-562 order now closes; the first raw root-literal mismatch moves to
record 131, the extra `-1278` record. Its two consecutive records have identical
trail length 3921, supporting (but not proving) speculative replay rather than
two committed trail entries. Collapsing consecutive rows with identical
literal, reason and trail length yields 149/149 identical literals and reasons;
this sensitivity check is not proof that the extra operation is speculative.
Tactix trace SHA is
`715f209b3fd8af78d0237403fe1c00239ed57ab6ab7547439e9fa72c86ddd472`.
All 77 CTests pass in 5.73 seconds on clean Release
`a8868f435ddef9a4c6d65c3b852ffe3ac4fa015dc23e3e96187927d1cb197b36`.
An initial
build invocation named nonexistent target `test_proof_emit`; the actual
proof gate is `sweep_proofs`, which passes. No solver failure was reproduced.
No candidate includes a formula-dependent solver-policy gate.

5328/job 1668647 reuses the immutable 5327 trace pins through the previously
declared 50,000--75,451 decision window (stop at 75,452). It separates ordinary
decisions from warm-up decisions and rejects truncation. Both tasks complete
and clean up. All 77,627 ordinary decisions still match, but the first warm-up
mismatch remains level 24, decision counter 187,082: `-2733` versus `14194`.
Combined counts are 78,245/78,247. Closing this root-order witness does not
close that later heap tie; do not claim it is the remaining cause.
5329/job 1668649 compares the clean cumulative 5327 Release with c2 r2 using
the unchanged six-task Ranks 17/21/34 matrix and checked runner from 5318.
Hypothesis: restored root order removes the core-only Rank-34 regression
without losing Rank-17's work reduction or imposing material matched-counter
Rank-21 overhead. This single diagnostic wave cannot establish acceptance;
all six models and scratch cleanup are required. Both use the required full
width, one proof-on solver per task, 12 logical CPUs and 48,000 MiB.

5329 is terminal with all six models checked and scratch clean. Rank 17 improves from
248.356 to 111.653 seconds (1.921B to 0.769B propagations); Rank 21 worsens
from 111.145 to 138.989 (0.603B to 0.789B propagations). Rank 34 worsens from
559.057 to 895.149 seconds (1.323B to 2.033B propagations). Paired ratios are
0.449568, 1.250520, and 1.601177. Thus the candidate is not ready for production:
the closed root witness and Rank-17 work reduction do not establish a general
runtime benefit. These are single-wave diagnostics, not decision-grade timing.

5330/job 1668658 is a terminal six-variant follow-up on the same Rank-21 input.
All six SAT models check and scratch is clean. Current/core-only/physical-only
have identical counters: 56,206 conflicts, 45,390,047 decisions, 603,059,704
propagations (110.993/112.344/112.098 seconds). Sequential/combined/bridge-pose
have 77,398 conflicts, 64,713,156 decisions, 788,553,687 propagations
(139.090/139.090/139.794 seconds). Thus sequential per-pair rewriting is
sufficient to change this work; physical publication alone is not. This
isolates the next causal trace, not a decision-grade timing rejection or
permission for a formula-specific default. All variants reuse prior immutable
Release pins and their exact source patches; no solver code was changed here.

The earlier core-only Release also passes 77/77 CTests in 4.43s.
The first attempts lacked tools/checkers/plan paths in the minimal source copy;
restoring test-only dependency links fixes those harness errors. No solver,
model, or proof failure was reproduced.

## Next actions and limitations

Proof obligations, three-wave timing confirmation, cumulative B28+C5 gate,
and one Claude-owned consolidation fuzz campaign remain required before any
production promotion. Broader matched-work cost and memory remain open.
Localize the sequential rewrite's first Rank-21 clause/order
change against the reference. Do not add more pose machinery on the assumption
that the bridge adjustment caused its regression. Keep the unresolved later
heap tie separate from this now-closed root witness.

Core metadata is currently an investigation implementation. Its storage and
copy costs need measurement; reproducing clause order is not permission to
accept a new component-level implementation penalty. The main solver source
is unchanged from the fuzz-passing `c2a95a6` checkpoint.

Primary artifacts: campaigns 5316/5317 `candidate.patch`, `manifest.json`,
comparison scripts and JSON summaries; the corresponding root-owned isolated
source copies and build pins remain local. Raw traces and logs are retained.

## Checkpoint hygiene

The three generated fixed-probe files `core-capture-proof-output/unit.cnf`,
`unit.drat`, and `unit.lrat` (74 bytes total) were removed after independent
checking. `core-capture-proof.cpp` retains the exact reproducible fixture.
No campaign pin, unique trace, baseline, active checker scratch, or fuzz-owned
artifact was removed. The documentation-size check reports zero errors over
all 30 budgets; four near-cap warnings remain. The progress checker reports
zero errors; its queue-empty warning came from sandbox-denied SLURM access,
not an empty queue. Exact authorized job queries govern current state.
Only 5314 tasks 35, 42 and 43 remain active at handoff. The frozen diagnostic
source patches intentionally contain 382 blank context-prefix lines, and
three immutable TSV matrices contain 21 CSV-writer CRLF rows. These account
for every staged whitespace-check notice; source/scripts/docs/JSON/job files
pass the whitespace check. Do not rewrite hash-pinned artifacts to remove
their format syntax. Andrew's accompanying three-page TeX/PDF report was
compiled twice without LaTeX warnings.

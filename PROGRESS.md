# Progress and resume guide

## Active ordered goal

Complete the two acceptance phases in order. Phase 1 requires independently
checked Rank 1--50 at 50/50 below 8,115.735 solver seconds and B28+C5 at 33/33
below 952.263 solver seconds, with every case within 1,200 seconds and 32 GiB.
The general Rank 1/6 preprocessing repair must remain effective. Phase 2 then
requires independently checked WV100 100/100 under the same per-case limits,
while preserving Phase 1. Phase 3 (user, 2026-09-20): solve each of the 89 fixed
WV2-89 instances (`benchmarks/tiers/WV2-89.tsv`, plan gate T4-WV2-89) within 600
seconds and 32 GiB with checked models or proofs, by studying each row's owning
solver and integrating its technique as a shared rule or strategy; rows that
several Kissat configurations solve are a DefaultPolicy parity question, never
per-instance tuning. Phases 1 and 2 stay preserved.

All mechanisms must live in Tactix's shared typed rule and strategy surfaces.
No sequential portfolio, external solving stage, benchmark/rank/filename
dispatch, or arbitrary benchmark tuning qualifies. Preserve N1--N4, native
DSR-to-LSR-to-Trestle verification, original-CNF SAT model checking, immutable
authorities, and Claude-owned default/random-policy fuzzing.

## Current checkpoint

The AM310 production source is frozen at
`build/20260919-am310b-dense-promotion-authority.FIexVn`. Its manifest records
base `fb1cc3bc`, patch SHA-256 `a0c14964...430c`, Release binary SHA-256
`2fc40073...d92`, corrected `Debug -O1 -D_GLIBCXX_ASSERTIONS` binary SHA-256
`2805ccc1...41f`, and the unchanged checker pins (`dsr-trim 3b5c7737...dee`,
Trestle `19bf79f4...945`). The earlier `am310` source bundle is superseded
because its patch was not applied inside the copied tree; its Release binary is
still the exact immutable binary used by AM311. At freeze time, all 53 intended
source/test/doc paths byte-matched the root worktree. The two
experimental checker edits are outside that source set and are not accepted.

AM310 repairs fuzz seed `297309643` generally: every redundant-to-irredundant
large-clause promotion now publishes the clause into all attached dense
occurrence views before later in-place rewriting. The exact Release and Debug
reproducer passes, corrected Debug corpus 026 solves and verifies in 17.689s,
focused Debug tests pass, and the full Release suite passes 169/169 in 132.39s.

AM311 jobs `1706635` (Rank), `1706636` (B28+C5), and `1706637` (WV100) use one
frozen Tactix binary, the original checker pins, native DSR, anonymous scratch,
and c12/c12/c8 allocations on `cpu-dense-hwulab-q`. All solver records are
complete with no timeout or abnormal exit:

| gate | solver coverage | solver seconds | largest solve | peak solver RSS |
|---|---:|---:|---:|---:|
| Rank 1--50 | 50/50 | 7,673.742 | 621.475s | 15,663,700 KiB |
| B28+C5 | 33/33 | 873.609 | 306.433s | 3,284,872 KiB |
| WV100 | 100/100 | 2,337.978 | 924.852s | 2,090,312 KiB |

Rank 1 is independently final at 1.018s. Rank 6 solves in 40.035s; structured
startup costs 0.207s and the declined FastEl check costs 0.139s, so the former
1,116s preprocessing failure is absent. At the latest audit, independent final
records stand at Rank 47/50, B28+C5 32/33, and WV100 95/100 with zero invalid
records; the nine remaining tasks are all in the frozen proof pipeline.

Claude session `69701055` terminally failed one deterministic Release
random-policy case: seed `296485266`, 77 variables / 329 clauses. The frozen
strict `dsr-trim --delete-implied-units` rejected a later SR line after Sweep
deleted a proof-only RUP clause which the checker had chosen as an implicit-unit
reason; the same proof passed without the strict option. AM312 fixes the solver
side generally by retaining native-DSR Sweep general-plan RUP prefixes while
preserving DRAT/LRAT cleanup. Fresh Release and assertion-enabled Debug binaries
produce the same UNSAT trajectory and byte-identical 520,107-byte proof for the
case; frozen strict dsr-trim and Trestle both verify it (job `1707099`). Two
small native-DSR Sweep cases plus their DRAT/LRAT variants pass their real
checkers (job `1707094`). The read-only authority is named by
`build/am312-authority.path`; its Release SHA is `2848a9cc...23d`, Debug SHA is
`b291eb11...805`, and checker pins remain unchanged. Full Release CTest passes
169/169 in 120.97s (job `1707113`); the 329-clause case is retained as corpus
entry 031 after Claude's prior 2,000-evaluation shrink found no reduction.
Claude session `41c62692` owns the complete repaired-pin rerun. The storage
guard remains active at 700 GiB.

AM316-AM320 (superseded, all uncommitted): AM316's Claude fuzz gate found two
Debug random-policy failures. Seed `296716882` (proof rejected under the old
forward pipeline) led to Codex's AM317 kernel DSR-shadow closure, which AM321
removes together with the shadow state it served. Seed `296725965` aborted at
the BVE definition-unit precondition because the canonical BVE round kept
attempting candidates after a growth-declined one had installed a unit
resolvent; AM319 asserts the candidate contract at `bve_eliminate` entry and
ends the round with `Ok` at the first candidate that changes state (corpus 035,
causal unit test). AM317's value-based Substitute unit derivation was removed
at the user's direction after a same-window pin attribution
(`benchmarks/runs/20260919-am318-t105-attribution`) showed it alone moved
`T105.2.0` from 342,205 to 511,237 conflicts; its hunks are preserved as
`am317-substitute-value-units.patch` there. The AM320 pin passed a 623,060-case
fuzz gate and all three gates with every conflict count equal to AM311, under
the old forward pipeline whose eight largest proof checks were cancelled
unfinished after ~10h; campaign READMEs under
`benchmarks/runs/20260919-am3{18,20}-*` hold the record.

AM321 (uncommitted) follows the user's directions of 2026-09-20. The
proof-checking pipeline is now exactly the Satsuma+Kissat reference, parameters
included (`dsr-trim <cnf> <dsr> <lsr>` with no options, i.e. backward and
core-trimmed; `dsr-compress -d`; Trestle on the text LSR, Trestle being the
authority), defined once in `tools/bench/proof_oracle.py` and used by the gate
runner, fuzz harness, proof suites, `run.py`, the portfolio tool and the
competition check. The old `-q -f -c --delete-implied-units` forward elaboration
made gate checks 50-160x slower than the owners' (`T105.2.0` 376s vs cancelled
unfinished after ~10h; latin_square_10.15 119s vs 12,686s) and hid emission
defects; evidence in `benchmarks/runs/20260920-am320-reference-pipeline-probe`
and `...-am320-reference-pipeline-fuzz-probe`. Proof emission was repaired
without touching search: Sweep's lifted prefix lemmas are written conditioned
on their target, as the BVE definition lifter writes `(unit v L)`, and the
proof-only DSR shadow propagator plus AM317's kernel binary-publication closure
are removed as unreachable; `kernel_propagate` is the one home for publishing
forced root literals; and no witnessed line may propagate when the checker
reads it: backward dsr-trim attributes a refutation closed by a witnessed
line's own propagation to that line and omits the hint groups of the clauses it
used, so Trestle rejects a valid step. An X7 clause that is unit at the root
therefore takes the serializer's general extension encoding and enters by RUP,
identically for certified and validated applications (13-clause reproduction,
corpus 036, and both directions pinned in `micro_proofs.sh`). Runs with and without a proof are now
identical by construction: Sweep's no-proof commit helpers and Congruence's
three `if (!k.proof) return true` early exits are gone, so both modes take the
certified path and only writes are conditional (the old forks diverged on
`DivS_862_11` and `b19_1` and on an eight-clause fixture). Also landed: a BVE
one-instance decline that changed state reports `Ok`, with DefaultPolicy's
`root_assignments` side-channel removed and round progress keyed on
`bve_eliminated`; `TransitiveReduce`'s scratch re-initialisation clears its
index lists (hardened Debug aborted on 3 of 5 probed Tier B instances); the
sub-solver end-pointer idiom; and `test_simon_model`'s self-refuting fixture.
AM322 (SR probe cost, BVE result read from `kernel_change_mark`) passed B28+C5
33/33 valid at 877.7s with conflict counts equal to AM311, and its Rank/WV100
records so far are all valid with equal conflict counts
(`benchmarks/runs/20260920-am322-change-mark-{b28c5,rank1-50,wv100}`); `oski`
(1,520 certified SR units) verified under the reference pipeline in 3,093s. Its
fuzz matrix was cancelled at the user's direction to fix every known
proof-production issue first. AM324 does that: the X7 encoding above replaces
the per-unit consequence probe (whole-formula snapshot per unit, SR 449 ms ->
1,129,025 ms on `oski`); two avoidable Sweep declines are closed, the unbacked
4,096-step capture bound (measured: every `DivS_862_11` refusal was a 4.4k-6.3k
step derivation) and the demand for original proof ids outside LRAT
(`antecedent-missing`); and because backward elaboration never examines
non-core lemmas, the fuzz harness and both proof suites also run forward
elaboration (`every_lemma_dsr_stages`; 63/63 corpus proofs accepted). The plan
states the reference pipeline. Verified on the frozen checkers (`1709322`,
`1709343`): corpus 42/42 with the audit in Release and Debug, recorded and random
policy; parity 93/93; proof suites PASS; failure-seeds 109/109 and controls OK.
Live: gates `...-am324-proof-production-{rank1-50,b28c5,wv100}` jobs `1709359`,
`1709382`, `1709383`; fuzz matrix `...-am324-proof-production-claude-fuzz` jobs
`1709653`-`1709668`. B28+C5 is complete: 33/33 valid, 869.0s, one conflict-count
change (`DivS_862_11` 178,468 -> 155,740, the removed bound). Fuzz PASS: Release legs on AM324 (400,000
nightly cases, focused 42,000 + 28,000, AVX2 9,000) and Debug legs on AM325
(50,000 + 50,000, focused 21,000 + 14,000, AVX2 9,000), 0 failures,
failure-seeds 109/109, controls OK. The one failure on the way, five Debug aborts
in `dbg-random-focused`, was the new "proof holds no root unit the kernel lacks"
invariant asserted in an inconsistent kernel after a Backbone replay closed the
refutation; AM325 scopes it to a consistent kernel (`test_backbone` regression),
Debug-only, Release byte-identical (`34bcac0b...`). Gates on that binary: Rank
49/50 valid 7,431.8s, B28+C5 33/33 869.0s, WV100 96/100, no regression; five
large certificates (`oisc`, `ncc`, three Tseitin) are still in the checker
(jobs `1709359_21`, `1709383_{79,82,85,89}`). Evidence:
`benchmarks/reports/2026-09-20-am324-reference-pipeline-proof-production.md`.
Scripts and
diagnostics in `benchmarks/runs/20260920-am32{3-nonpropagating-witness,4-proof-production}`.

Open items: (1) the Sweep default change moves DefaultPolicy where the bound
used to decline, so AM324 is judged on the gates, not assumed neutral.
(2) Tseitin/parity certificates are 8-33x larger than LymphoSAT's
pseudo-Boolean ones; this is structural for clausal DSR and is a size/check-time
matter, not validity. (3) A witnessed clause that is already false at the root
cannot avoid closing the refutation at its own line; no occurrence is known.

## Phase 3 status (WV2-89)

Baseline on the committed binary (`benchmarks/runs/20260920-am325-wv2-89-baseline`,
table in its `table.md`): 72 rows within 600s, 5 late, 12 timeouts.

AM329 (Release `74eeb53b...`, Debug `9a7165ba...`; report
`benchmarks/reports/2026-09-21-am329-negation-first-entry-floor.md`) adds the owner's
negation-fixing pass before FixAndCut (typed `negation_fixing_first`; the owner's cut
point and unconditional handoff are typed but declined) and a one-generator entry
floor for generic symmetry discovery. Gates `...-am329-negation-unit-sign-*` (jobs
`1711797`-`1711800`): Rank 50/50, B28+C5 33/33, WV100 100/100, WV2-89 79 solved / 74
within 600s (gained `bphp_p23_h22`, `xor_op_n40_d3`), no lost row; fuzz matrix
`...-am329-negation-unit-sign-claude-fuzz` 18/18 legs, 0 failures. It supersedes AM327,
whose pin was UNSOUND although every gate verified (planner root values disagreed with
the published negation unit); Debug builds now cross-check every chain/orbitopal-
certified X7 instance. Large certificates: on the AM324 gates five solved rows are NOT verified inside the
57,600s window. `tseitin_grid_n250_m250`, `tseitin_d3_n100000`, `tseitin_grid_n400_m400`
and `ncc_none_21015...` pass `dsr-trim` (3,153s / 14,316s / 53,938s / 8,732s) and time
out in Trestle; `oisc-subrv-sll-nested-8` times out inside `dsr-trim`. Owners: the
Tseitin rows and `ncc` are LymphoSAT's (VeriPB, verified; every Kissat variant is also
unverified on `ncc`); on `oisc` Satsuma+Kissat's own DSR check times out at 45,000s
while Kissat's DRAT check verifies in 4,271-7,814s. Work item: parity-refutation proof
size on the Tseitin rows. The same five AM329 rows were cancelled by job ID and count as not verified
(`NOTE-cancelled-checks.txt` in the two gate directories).

Remaining WV2-89 misses (15): `ramsey_4_4_19` is a symmetry-breaking-predicate gap
(Kissat solves original + Satsuma's clauses, times out on original + Tactix's;
Satsuma's tail after the eight common units is what matters; evidence
`benchmarks/runs/20260921-wv2-owner-anchor`); `mp1-Nb7T42` and `SC25_Timetable` receive
the owner's preprocessing and are search gaps; `6g_6color` and `hcp_CP18_18` are
large-domain (>50k variables) preprocessing gaps (reopened, see next action); LymphoSAT structural families
(`sgen1-sat`, `sted5`, `1-ET-512`, `FmlaEquivChain`, `noL-11-4`, `puzzle32`); plain CDCL
parity rows (`snw_16_9`, `Circuit_multiplier24`, `ntil-90d-39`, `lockchart...L19`).

## Learned scheduling candidate: when to break symmetry

User direction: *when* (and how strongly) to apply symmetry breaking is a decision to
learn (plan §8.5), not a hand-tuned gate. Evidence,
`benchmarks/runs/20260921-wv2-anchor-cut-screen` (WV100+WV2-89, three same-window arms):
the owner's cut point with its first-literal fix anchor gains UNSAT rows
(`ramsey_4_4_19` timeout -> 290s, `bphp_p23_h22` 489s -> 1.8s) and loses or slows SAT
rows (`ntil-90d-37/40` lost, `noL-11-0/8` 2-3x slower). On `ntil` the whole difference
is one cut binary, `(-323 v 306)` on early-decided variables against `(-2 v 19)`.
Satsuma+Kissat itself loses SAT rows plain Kissat solves (`ntil-90d-37` 5,000s vs 223s,
`noL-11-*` 1.5-2.6ks vs 0.1-0.5ks). SAT/UNSAT is unknown up front, so no static rule
separates the two groups. Consequences: every symmetry choice stays a typed,
API-selectable action (`negation_fixing_first`, `cut_point`, `first_literal_fix_anchor`,
nonforcing-cut admission, structured handoff) so a learner has the action space; no
gate is tuned on the deciding rows and nothing dispatches on benchmark identity. Late
(search-time) symmetry breaking is not free under N2: an SR line must also repair every
learned clause in the checker's database.

## Exact next action

Two same-window scratch experiments (env hatches in
`benchmarks/probes/scratch/wv2-owner-first-literal`; the tree holds only the typed,
default-off `first_literal_fix_anchor`/`sr30` and the tick-charged negation scan, CTest
green in job `1712230`, uncommitted):

1. `benchmarks/runs/20260921-wv2-cut-phase-screen` (jobs `1712722`/`1712723`): does a
   phase hint (a cut's base prefers false, carried until H attaches) let the owner's
   anchor + cut point keep the SAT rows? Round 1 was vacuous (H attaches after
   structured startup, the hint never landed).
2. `benchmarks/runs/20260921-wv2-large-domain-dig` (jobs `1712749`/`1712750`):
   `hcp_CP18_18`. Tactix gates generator binary fixing on a negation unit; the owner runs
   it unconditionally (0 negation units, then 1,729 binary-fixed literals). Arms: keep the
   work-capped generator prefix, ungate binary fixing, both; side effects on `6g_6color`,
   `SC25_Timetable`, `mp1-Nb7T42`.

A change that fails alone is not shelved: find the coupled change (user direction). Run
the Debug corpus and focused fuzz legs before any gate.

## Recent changes

- Acceptance checking is the exact Satsuma+Kissat DSR pipeline plus a forward every-lemma audit; X7 root-unit clauses take the non-propagating general encoding.
- Sweep: target-conditioned prefix lemmas, proof/no-proof parity, unbounded capture default, proof ids required only under LRAT.
- BVE candidate contract with the result read from `kernel_change_mark`; TransitiveReduce scratch reset; Debug invariant scoped to a consistent kernel.
- Fixed the WV2-89 tranche (`benchmarks/tiers/WV2-89.tsv`, plan gate T4-WV2-89) as Phase 3.
- AM329: owner negation fixing before FixAndCut, generic-entry generator floor, Debug cross-check of certified X7 rows; AM327 withdrawn as unsound.

# Search-time algebraic reasoning

This is the detailed algebraic increment of [Development Plan §9.5](../DEVELOPMENT_PLAN.md#95-organic-winner-technique-integration). N1–N4 and every standing gate in that plan remain binding.

GF(2) is the immediate integration and CG-performance priority. The existing
GF(3) and unweighted-cardinality verticals remain in scope, default-off until
their gates pass, and must not be removed while that work is prioritized.

The currently supported public certificate architecture is the
[algebraic proof bundle](algebraic-proof-bundle.md): batch specialized lemma
checks behind one original-CNF binding, then check one canonical ordered DSR
consumer proof against exactly those lemmas. GF(2) may use XLRUP internally;
cardinality and GF(3) retain clausal DSR fallback until equally strong native
bindings exist. This is one user-facing certificate and acceptance decision,
not independent proof modes whose conclusions are assumed by one another.
Add whole-run PBP as an explicit additional mode: pinned VeriPB checks and
elaborates it, then verified CakePB checks the elaborated proof against the
original CNF. The new mode must cover every proof event a selected run can
produce. It does not replace Automatic, DSR, DRAT, LRAT, Trestle, XLRUP or the
bundle route, and cannot narrow the N1 action surface.

## Frozen CG benchmark

The [CG collection](../benchmarks/collections/CG/README.md) freezes 25
independently checked CryptoMiniSat 5.14.7 solves from the pinned Tier-D rerun.
It includes all 21 sub-600-second exclusive or at-least-10x wins found in the
601 published rows, plus the strongest CMS-over-baseline row from four
previously unrepresented families. The collection spans 14 families and binds
each compressed input and checked result by SHA-256. Its long-form comparison
records CMS and all seven saved baseline solve times and statuses for 200
solver-instance rows.

The rerun stopped at 601/1,166 after representative evidence was accepted, so
CG is not a complete Tier-D census. It is the fixed mechanism and performance
target: one instance-independent DefaultPolicy should solve every row within 2x
the checked CMS time at matched resources. N1--N4, independent proof/model
checking, held-out nonalgebraic coverage and the standing regression gates
remain mandatory. No generated proof or expanded CNF is retained.

After the corrected 5,000-wall-second proof-free CMS census is terminal,
reassess rather than automatically enlarge CG. Keep 25 rows as the default
development budget and select for family and mechanism breadth, SAT/UNSAT,
exclusive/major-win status and runtime scale; avoid near-duplicates from a
single prolific family. Replace a row only when the full census supplies a
materially stronger or more representative target. Model-checked proof-free
SAT rows may support selection directly. Proof-free UNSAT is discovery evidence
only and must receive a targeted proof-producing independent check before it
can enter CG. Any changed selection must regenerate and verify the manifest,
input hashes, eight-arm runtime table, rationale and no-duplicate-input contract.

**Search-time algebraic reasoning.** Extend the retained authenticated GF(2)
matrix, existing GF(3) equations and exact unweighted cardinality constraints
to reason over the live CDCL trail at propagated search fixpoints. Derive a
conflict before an implication, with exact source provenance and the assigned
antecedents in a globally entailed clause. A non-root conflict remains an
ordinary conflict clause; only a root contradiction may publish the empty
clause. Certify and install the clause before CDCL consumes it, then use
ordinary propagation and conflict analysis. Synchronize retained state across
backtrack, reassignment and restart; source deletion, substitution and
elimination require durable certified premises or invalidation. An interrupted,
stale or exhausted service publishes nothing. Expose bounded search actions,
profiles, replay and raw feedback through the public strategy interface;
strategy owns scheduling and resource allowances. Charge deterministic work,
memory and proof costs, and avoid repeated unchanged-trail work. Preserve the
restart-root service as a control. Promote the service to DefaultPolicy-on only
after focused proof/model tests, effective independent fuzz, full Debug,
B28+C5, Rank1–50, WV100, WV2–89 and Tier-D gates on one coherent candidate.
Use the CryptoMiniSat discovery cases and mixed/nonalgebraic held-out inputs
to demonstrate consumed consequences and acceptable total overhead. Weighted
pseudo-Boolean and general polynomial reasoning are outside this increment.
The pinned mdp diagnostic established a concrete GF(2) cost gate: CryptoMiniSat
used a 346-by-374 Gauss matrix and solved in 8.5 s; disabling its matrices
exceeded 300 s, while Tactix emitted 17 million proof additions in 120 s and
timed out. Reuse authenticated matrix-row certificates or another independently
checked clausal derivation across nonroot reasons, bound proof work by policy,
and demonstrate a matched solving improvement before enabling the service.
A [second checked mdp winner](../benchmarks/reports/2026-10-17-affine-mdp284-256mb/README.md)
reproduced the cost gap on the coherent 256 MB candidate: CMS proved UNSAT in
7.225 s; Tactix admitted 305 sources and published 1,928 matrix reasons, but
spent 107.7 s in affine overhead and returned UNKNOWN at 120 s after 17.49
million proof additions. The original-CNF scratch was deleted.
A proof-free same-binary diagnostic admitted no GF(2) sources because the
service requires a live proof owner, so it cannot isolate proof cost; retain
the measured 107.7-second affine overhead as the valid bottleneck evidence.
The [matched nine-task screen](../benchmarks/runs/20261017-affine-live-resume-performance/analysis.json)
now makes the gap concrete: old opt-in, row-resume opt-in and new default all
timed out at 120 seconds on checked CMS `mdp`, `par32` and `lightsout` rows.
On `mdp`, row resume emitted 17,610,316 affine proof additions versus
17,085,463 old (+524,853); it does not close the proof-cost gap. On `par32`
and `lightsout`, the opt-in paths recognized 1,318 and 529 XOR groups but
admitted zero matrix sources. Each reports one incomplete-image root attempt
and one exhausted authenticated-subimage attempt, so their immediate obstacle
is the bounded admission path. A [pinned admission attribution](../benchmarks/reports/2026-10-17-affine-admission-attribution/README.md)
now shows the specific 100 MB retained-source limit is too small: preflight
requires 217,524,715 bytes for `par32` and 142,743,316 for `lightsout`.
The [opt-in 256 MB dose](../benchmarks/reports/2026-10-17-affine-admission-attribution/README.md)
passed its focused proof/model test and paired zero-retention audit. It
admitted 625 `par32` sources but timed out at 120 seconds after nine reasons.
On the direct `lightsout` row it admitted 508 sources and produced an
independently checked original-CNF UNSAT proof in 1.519 seconds, versus pinned
CryptoMiniSat's 7.425 seconds. This is one checked speed win, not DefaultPolicy
promotion or CG qualification. A [same-binary four-row lightsout audit](../benchmarks/reports/2026-10-17-affine-lightsout-256mb/README.md)
verified two UNSAT proofs and two SAT models; all four Tactix times are under
2x the checked CryptoMiniSat rerun. The other three rows admitted zero GF(2)
matrix sources, so their fast solves cannot be credited to this service.
CryptoMiniSat's no-matrix totalizer UNSAT control timed out at 300 s versus
10.852 s with matrices. Full N1--N4 gates remain open.
The first current-source full gate passed 366/368 Debug tests; a missing
environment inventory entry and stale wrapper-hash test fixture caused two
failures. Both [focused repairs](../benchmarks/reports/2026-10-17-affine-admission-full-gate-repair/README.md)
pass; the coherent rerun passed 368/368 Debug tests, Release `micro_proofs`,
40 runtime-harness tests and the 785-file source/checker pin.
Two [original-CNF lightsout XOR refutations](../benchmarks/reports/2026-10-17-lightsout-original-xor-refutation/README.md)
confirm missed structure in both direct and totalizer UNSAT encodings. From
529 and 2,475 complete original XORs, respectively, 184 and 882 selected rows
sum to `0 = 1`. Pinned FRAT-XOR/CakeXLRUP verified 32,923- and 61,939-byte
XLRUP proofs, rejecting a missing-XOR control for each. The two checked SAT
lightsout rows have consistent original XOR systems. These are offline
certificates, not Tactix solves. Prioritize bounded admission and compact
emission before repeating the CMS versus Tactix comparison.
All nine rows passed pin and scratch-cleanup audits; none is CG qualification.
The pinned CryptoMiniSat [Gaussian reason path](../tools/solvers/variants/cryptominisat-5.14.7/src/gaussian.cpp)
retains a clausal reason, emits one derived XOR with source XOR IDs, then
emits a clause implied by that XOR. This avoids Tactix's current full clausal
expansion at search time. Evaluate a compact certified XOR route as a separate
speed candidate if row resume cannot close the matched gap. The pinned
[FRAT-XOR/XLRUP format](../tools/checkers/frat-xor-855f3d0/format.md)
supports XOR and RUP steps but not RAT/PR, so a route that mixes these XOR
lemmas with ordinary Tactix DSR/SR search needs a checked composition bridge;
the compact fixture alone does not certify that bridge. A pinned-checker
control with the fixture's final empty-clause step removed reports
`empty clause not derived at end of proof`, so an XLRUP lemma-only run cannot
be counted as verified UNSAT. GF(3) and cardinality
still require their own proof paths or independently checked clausal fallback.
A [SAT-source selector challenge](../benchmarks/reports/2026-10-17-xor-lemma-batch-bridge/README.md)
now proves two globally entailed XOR-derived clauses in one pinned XLRUP
check and rejects a false-lemma control. This suggests a compositional
certificate: verify all algebraic lemmas through a selector challenge built
from the original CNF, then independently check the ordinary DSR refutation
against that CNF plus the same lemmas. The production identity binding,
complete mixed proof and cost gates remain open.
A [mixed composition fixture](../benchmarks/reports/2026-10-17-xor-lemma-mixed-composition/README.md)
now checks the second stage as well: CakeXLRUP verifies both XOR lemmas from
a satisfiable source subset, and independent every-lemma DSR checks a
refutation against the original CNF plus those exact lemmas. The same DSR
proof fails without the lemmas; source-prefix, selector and augmentation
bindings are audited. This is a small proof-engineering result, not yet a
streaming solver certificate or DefaultPolicy speed claim.
The reusable [two-stage checker](../tools/bench/check_xor_dsr_composition.py)
constructs the selector and augmented CNFs from one lemma list, checks source
premises as a multiset subset of the original, requires CakeXLRUP and
every-lemma DSR acceptance, and deletes intermediates. A positive fixture
and three negative binding controls pass. It is checker infrastructure; the
solver still needs compact emission and matched performance evidence. The
composer directs FRAT-XOR's elaboration temp into its deleted scratch area;
the binding controls also reject a leaked temp beside the source proof.
A [nontrivial consumer fixture](../benchmarks/reports/2026-10-17-xor-lemma-nontrivial-consumer/README.md)
checks a satisfiable XOR source that entails a non-BCP unit, then an ordinary
DSR RUP addition and refutation that require that exact lemma; the same DSR
proof fails without it. This strengthens the mixed-format check beyond an
immediate empty-clause consumer but still does not cover a live solver stream.
An [authenticated-record converter](../tools/bench/affine_record_xor_challenge.py)
validates complete source XOR encodings, physical occurrence identity, an
original-CNF clause subset and the algebraic reason before writing a compact
FRAT-XOR selector challenge. Sixteen relabeled unit reasons, one four-literal
reason and a compressed original pass pinned CakeXLRUP; four malformed records
are rejected.
The [real mdp capture](../benchmarks/reports/2026-10-17-affine-real-reason-capture/README.md)
now validates the route on an actual level-6 Tactix search reason. Its 87
retained source rows contain derived clauses, so the original-only converter
rightly rejected them. Independent original-CNF recovery found 374 complete
XOR encodings; 97 original XORs entail this width-15 reason. Pinned FRAT-XOR
and CakeXLRUP verified the compact challenge from 748 original clauses using
9,701 XLRUP bytes, while the live row had 59,682 clausal proof events. A
one-literal false-reason control remained satisfiable under the selected XORs
and was rejected by CakeXLRUP. The [full 33-reason audit](../benchmarks/reports/2026-10-17-affine-real-reason-capture/all-core-verification.json)
then found every source row in the original XOR span. Only two exact reason
clauses match their XOR rows; 31 are weakenings. All 33 XOR core clauses passed
pinned FRAT-XOR/CakeXLRUP from original source clauses, and all 33 exact live
reasons passed independent every-lemma DSR as RUP weakenings. The core proofs
were 5,265--11,235 XLRUP bytes each, with scratch deleted. This is a captured
reason batch, not a complete solver proof or speed result. A [single 33-lemma
composition](../benchmarks/reports/2026-10-17-affine-real-reason-batch/README.md)
now binds all captured reasons in one 161,226-byte FRAT-XOR/CakeXLRUP proof
and a separate every-lemma DSR refutation of a constructed consumer; missing,
misbound and semantically false lemma controls fail; an independent model check
confirms the false-lemma consumer is SAT. It uses 230 original XORs and deletes all raw
scratch. The consumer is not the actual mdp search proof. Next, make
original-source decomposition and compact emission bounded within the live
service, compose emitted reasons with the full DSR search proof, and rerun
matched solve and proof checks.
An [opt-in compact matrix journal](../benchmarks/reports/2026-10-17-affine-compact-provenance/README.md)
now omits duplicated clausal proof-event telemetry while preserving the
reason and authenticated physical source records. Focused live and proof
tests pass, but the solver still emits the full clausal proof. The matched,
proof-on `cpu-q` screen completed: both `mdp` arms timed out at 120 seconds
with identical proof-addition counts, while both `lightsout` arms produced
independently checked UNSAT proofs (1.519 and 1.469 seconds). The fresh full
regression gate remains separate. This is journal-size preparation, not live
compact proof composition or CG speed qualification.
Pinned [mdp overlap](../benchmarks/reports/2026-10-17-affine-mdp-source-overlap/README.md)
and [proof-shape](../benchmarks/reports/2026-10-17-affine-mdp-proof-shape/README.md)
draws found 70% source intersection but negligible prefix/row-key reuse;
77% of 1.27 million additions were RUP derivations. The
[symmetric-difference resume](../benchmarks/reports/2026-10-17-affine-symmetric-difference-resume/README.md)
used 106 versus 185 incremental additions in one small fixture and passed
64/64 seeded every-lemma DSR and forward DRAT checks under ASan/LSan. Only
2/55 prior source sets were subsets, so live reuse must handle removal as well
as addition. Require an exact coordinate key (projection may change support
under one owner), authenticated proof epochs, charged retained support and
matched total-cost/solve gates before DefaultPolicy promotion.
Imported helper deletion also needs reference-counted release or a bounded
monotone DAG: proof epoch retention currently keeps clauses until flush.
A [64-seed monotone proof probe](../benchmarks/reports/2026-10-17-parity-monotone-resume/README.md)
passed independent every-lemma DSR and forward DRAT with all helper deletions
suppressed under ASan/LSan. It leaves the byte cap, epoch ownership and
full mixed-proof acceptance as mandatory gates.
An [isolated live-cache prototype](../benchmarks/reports/2026-10-17-affine-live-resume-prototype/README.md)
passed two full original-CNF proof checks on `cpu-q` with 260 versus 431
additions for one reused row. Its pinned patch is now a shared-source
default-off candidate whose [focused gate](../benchmarks/reports/2026-10-17-affine-live-resume-focused-gate/result.json)
passed four original-CNF every-lemma DSR and forward DRAT checks under ASan/LSan.
A [64-case shared-source row-resume fuzz](../benchmarks/reports/2026-10-17-affine-row-resume-fuzz/result.json)
passed both independent checkers with 20,560 resumed additions versus 31,108
fresh; a [removed-source stale-boundary test](../benchmarks/reports/2026-10-17-affine-live-resume-stale-boundary/README.md)
declined publication without changing proof state. Epoch, projection, broad
nonroot fuzz and matched solver-time gates remain. The
shared tree-resume prerequisite passed the [368-test Debug and Release
micro-proof gate](../benchmarks/reports/2026-10-17-parity-resume-full-gate/job-1787654.log).
An [isolated removed-source live gate](../benchmarks/reports/2026-10-17-affine-live-resume-removal-v2/README.md)
also passed four original-CNF proofs on CPU-q: a non-BCP source swap used
247 versus 302 fresh additions. The shared candidate and broad fuzz remain
under verification; these fixture gains are not a solver-time promotion result.
The [minimum-residue ordering screen](../benchmarks/reports/2026-10-17-affine-min-residue-pair/README.md)
raised mdp total additions 1.34% and overhead 2.49% without solving; it was
removed, while its four-row proof regression remains. The
[persistent nonroot proof fuzz](../benchmarks/reports/2026-10-17-affine-persistent-search-proof-fuzz/README.md)
narrows N2 risk but does not relax speed gates. The
[compact FRAT-XOR fixture](../benchmarks/reports/2026-10-17-frat-xor-clausal-bridge/README.md)
checks CNF-to-XOR and XOR-to-clause steps, including rejection of a wrong XOR;
any alternative proof route must independently verify the complete ordered
mixed DSR/RAT/SR stream under N2. The subplan owns Rank 569's proof target.
The first GF(3) vertical reuses public X19 after root owner capture; its
nonroot clause rearm and DSR proof have focused checks, but scheduling,
cardinality, fuzz and acceptance remain open.
The unweighted-cardinality vertical starts with bounded complete-subset CNF
authentication and a conditional clause core. The first live adapter rechecks
physical source clauses and proof IDs at commit, emits bounded resolution-only
RUP, and installs a nonroot reason before ordinary BCP; its focused proof and
fuzz checks pass. A typed public rule (ID 46), online signed-bound payload,
replay and C v17 configuration now permit explicit nomination of bounded
complete-CNF systems; a C v17 root refutation passes independent every-lemma
DSR checking. General
discovery, policy scheduling, and the full gates remain before this milestone
is satisfied.
A [64-case non-BCP cardinality proof fuzz](../benchmarks/reports/2026-10-17-cardinality-resolution-fuzz/result.json)
now passes independent every-lemma DSR and forward DRAT under ASan/UBSan
with leak detection. It relabels one two-constraint, nonroot implication and
checks that BCP alone cannot refute any negated reason; it does not establish
general cardinality discovery or DefaultPolicy performance.

The explicit whole-run PBP successor gate found and repaired two streaming
boundary defects. First, simplification could delete a binary reason for a
propagated root literal before the proof published that unit, leaving later
RUP steps unsupported in VeriPB. Second, publishing the unit during input
parsing interleaved internal clause IDs, and the `f` header incorrectly used
the largest ID as the original constraint count. Streaming PBP now publishes
such root units without changing solver-semantic `unit_ids` and counts each
raw input seed exactly once. The
[repaired successor](../benchmarks/reports/2026-10-18-pbp-broad-fuzz-r2/README.md)
passes 128 SAT models, 128 VeriPB+CakePB UNSAT proofs, 3,593 exact deletions,
128 rejected source corruptions and 128 rejected elaborated-kernel corruptions,
plus both focused reproducers. The immutable repaired-source gate
[1791145 + 1791146](../benchmarks/reports/2026-10-18-pbp-current-source-full-gate-immutable/README.md)
at `36ddc73d1` passes 367/367 Debug tests, Release `micro_proofs` 1/1,
40 runtime-harness and 33 proof-oracle tests, the campaign/Slurm harnesses and
all documentation/pin checks; final source/evidence hashes matched and the
Release binary is `0b89d912...45197a1`. The focused supplement repaired a stale
ignored checker symlink and explicitly forbids a skipped proof test. This
closes the fresh-source prerequisite. The preserved paper release
`0f7c10174` is now the controller-regression target with a 10% aggregate
solver-time allowance; correctness, retained solves, proof/model validation,
memory, per-instance limits and cleanup remain strict. The
[paper-target scores](../benchmarks/reports/2026-10-18-pbp-paper-target-regressions/README.md)
accept B28+C5 job 1791147 (+0.16%), Rank1–50 job 1791148 (+1.23%) and WV100
job 1791149 (-2.10%). The paper's one Rank proof censor and three WV solver
timeouts remain explicit inherited limitations, not independent verification.
Full Tier-D job 1791375 remains active; no PBP promotion is claimed before it
passes. Proof-free four-core CMS screen 1791200 is terminal with all 1,166 rows
published: 342 model-checked SAT, 296 unchecked UNSAT and 528 UNKNOWN, with no
result or cleanup failures. Because its `--maxtime 5000` limit was aggregate
CPU time, it remains screening evidence rather than the requested 5,000-second
wall-time comparator. Completed handoff 1792280 hash-bound the result inventory
and submitted corrected array 1792904. That exact continuation reuses 638 clean
SAT/UNSAT rows and reruns all 528 UNKNOWN rows on four cores and 32 GiB, without
proof production, under a monotonic 5,000-second wall limit. It waits behind
`afterany:1791375`, so it cannot compete with the active Tier-D gate.
Hash-checked terminal scorer 1792344 is dependency-blocked on all tasks in
1791375 and writes the paper-target base score after the array exits. Any
32 GiB checker memory failure still requires a source-bound 64 GiB recheck and
a second score before the full gate can pass.
All of the user's Slurm work is observed by reusable storage governor 1791687.
It samples global home usage every 600 seconds, dynamically discovers new jobs,
holds only PENDING tasks above 750 GiB, and releases below 700 GiB only tasks it
held itself. RUNNING jobs are never control targets. This governor supersedes
the narrower read-only Tier-D observer 1791376 and is operational protection,
not performance or correctness evidence.

The full Tier-D rerun also owns adjacent CryptoMiniSat mechanisms needed by
the one-policy CG target. A later causal replay refines the initial `mp1`
binary-distillation attribution: default CryptoMiniSat and a pass that only
preserved propagation-induced watch/search pose both solved in about 1.67 s,
while the disabled, shuffle-only and RNG-only arms remained UNKNOWN after
roughly 62--65 s. The pass retained 80,262 binaries but reported no logical
deduction, deletion or root assignment. The causal owner is therefore the
retained propagation/watch pose, not a binary-distillation consequence. Test
this as a public, proof-neutral, complete-literal propagation action with
explicit effort/replay and persistent pose; do not describe ordinary binary
deletion as the recovered mechanism. See the
[causal diagnostic](../benchmarks/reports/2026-10-18-cms-distillbin-causality/README.md).
The ternary Folkman win instead depends on SLS: two initial CCAnr rounds reduce
the unsatisfied-clause count from 18 to zero and seed CDCL phases; disabling
SLS returns UNKNOWN after 60.49 s with 1,291,995 conflicts instead of SAT in
1.25 s. Tactix must test a general bounded startup/local-search guidance
schedule against its existing Walk service, with complete models independently
checked and partial assignments used only as guidance. See the
[mp1/Folkman attribution](../benchmarks/reports/2026-10-18-mp1-folkman-cms-attribution/README.md).

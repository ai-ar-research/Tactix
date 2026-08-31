.TH "TACTIX PHASE 4.25" "31 August 2026" "Tactix development" "Report for Andrew"
.SH NAME
Fixed-Variable Active State and Rank 1--40 Revisit
.SH ABSTRACT
Tactix now matches Kissat's active-variable semantics: a semantic level-zero
fixed variable is inactive even when its assignment and stable identity are
retained for proof and model reconstruction; a non-root search assignment
remains active.  A uniform Rank 1--40 campaign solves all 40 instances in
10,290.269 seconds, improving the preceding 10,573.791-second candidate by
2.7 percent.  All 22 SAT models verify.  At this snapshot 16 of 18 UNSAT
proofs verify and the remaining two checks continue nonblocking; every completed
record is valid and residue-free.
.SH
Executive outcome
.PP
The repair is formula-independent and introduces no option, classification
consumer, benchmark/rank selector, or effort/admission gate.  Aggregate
geometric mean improves 2.6 percent.  SAT time improves 10.2 percent, while
UNSAT time regresses 4.2 percent, chiefly because Rank 33 grows from 395.771
to 753.767 seconds.  The increment is retained as reference-correct and
aggregate-positive; Rank 33 becomes the next downstream localization owner.
.SH
How Rank 17 exposed the mismatch
.PP
The preceding Rank-17 investigation localized a 104.637-to-385.402-second
trajectory change to per-candidate publication of BVE roots, retirements, and
propagation roots into the decision heap.  That lifecycle matches Kissat and
already made Rank 34's complete BVE removal sequence exact, so it remained.
.PP
A global event trace was not a usable observer: both solvers timed out at 900
seconds with header-only traces.  A Trace-only standalone observer now waits
for the first BVE schedule, writes one symmetric summary and optional members,
then exits before the first candidate.  It has no Release behavior.
.PP
Tactix schedules 328,081 variables and Kissat 328,795.  They share 322,663;
5,418 occur only in Tactix and 6,132 only in Kissat.  Among common members,
72,684 have different occurrence pairs and 320,706 different heap positions.
The schedule is already different before per-candidate publication.
.PP
At first BVE entry the conflict boundary is nearly shared (18,268 versus
18,269), but Tactix retains 616,255 root trail identities, 1,910,024
irredundant clauses, and 66,106 binaries.  Kissat has a 5,593-entry trail,
1,699,758 irredundant clauses, and 254,564 binaries.  Tactix's old 944,336
active count combined its 328,081 unassigned population with retained roots.
.SH
Reference semantics and repair
.PP
Pinned flags.c marks a fixed literal and immediately deactivates its variable:
it clears active, decrements solver->active, dequeues the variable, and removes
it from the stable heap.  Ordinary search assignment does none of this.
Tactix's kernel_active_variables now implements this semantic view without
discarding reconstruction state.
.PP
The complete production-consumer audit found exactly the existing Kissat
consumers:
.IP 1. 4
Factor's factordelay population predicate.
.IP 2. 4
Substitute's productive-round test.
.IP 3. 4
Probe and Eliminate outer fixed-point tests.
.PP
Two additional uses are Trace output only.  Physical heuristic membership,
variable collection population, and BVE occurrence membership remain separate
representations.  No new consumer was added.
.PP
Causal job 1657636 changes only active=944336 to active=328081 at first BVE;
all schedule, formula, score, decision, and propagation fields remain identical
to the Tactix-before authority.  Focused job 1657638 then solves Rank 17 with
a verified model in 311.993 seconds versus 449.150 before.
.SH
Homogeneous Rank 1--40 result
.PP
Job 1657639 uses immutable Release SHA
5743b0a65b76b3cc92d6011b9021744d231c0544cbfc35f14350d63aa5a03698
for every rank.  No diagnostic environment or comparator rerun is used.
.TS
center tab(|);
l r r r r r.
Measure|Candidate|Preceding|Ratio|Kissat|Ratio
_
Total seconds|10,290.269|10,573.791|0.9732x|7,675.236|1.3407x
Geometric mean|-|-|0.9739x|-|1.3303x
SAT seconds|4,520.427|5,034.213|0.8979x|4,057.914|1.1140x
UNSAT seconds|5,769.842|5,539.578|1.0416x|3,617.322|1.5951x
Solved|40/40|40/40|-|40/40|-
.TE
.PP
Largest improvements versus the preceding candidate are Rank 19
(452.403 to 166.486 seconds), Rank 31 (628.742 to 461.093), Rank 17
(449.150 to 314.130), and Rank 34 (807.219 to 692.338).  Rank 36 remains
closed at 385.209 versus Kissat's 368.510.  Rank 4 verifies UNSAT in 17.193.
Rank 32 remains UNSAT at 401.877, close to 401.623 previously and below the
older accepted 601.285 seconds.
.PP
Rank 33 is the material regression at 1.905x.  Smaller regressions occur on
Rank 37 (1.049x), Rank 6 (1.254x), Rank 22 (1.039x), and Rank 40 (1.031x).
They are downstream trajectory evidence, not grounds for a gate or rollback.
.SH
Memory and codebase revisit
.PP
Peak sampled process-tree RSS is unchanged at 16,987,860 KiB, still Rank 10.
The active repair changes accounting rather than physical storage and does not
remove retained clauses, watches, variable-indexed arrays, or proof/model
identities.  Rank 10 representation ownership remains the memory priority.
.PP
The revisit separates four concepts which should not be collapsed: semantic
active population, physical heuristic membership, collection population, and
BVE occurrence membership.  The production change remains deliberately small.
The bounded schedule observer remains diagnostic only, and its environment
names are absent from Release.
.PP
Release passes 76/76 and Trace 75/75.  N1--N4, proof accountability, the
proof-rule/strategy boundary, Kissat creation order, and uniform
BVE/Factor/forward-subsumption/Vivify defaults remain intact.  Dose5, Factor5,
NoBve, NoBveLight, and StartupFactorOnly remain absent.
.SH
Decision and next work
.PP
Retain the active-state repair.  Localize Rank 33 from its last common state
without an exception.  Continue Rank 17 upstream from its different physical
formula/root-compaction/search state, rather than changing BVE or Factor
admission.  Keep Rank 10 as the first portable memory target.
.PP
Detailed evidence is in:
.IP \(bu 3
benchmarks/reports/2026-08-31-fixed-active-rank1-40-codebase-revisit.md
.IP \(bu 3
benchmarks/reports/2026-08-31-rank17-bve-h-lifecycle-regression.md
.IP \(bu 3
benchmarks/runs/20260831-3760-tierd-ranks1-40-fixed-active/comparison-live.md
